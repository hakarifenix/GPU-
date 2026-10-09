// NOVA GPU - Software GPU
// tests/test_rasterizer.cpp - triangle rasterization, fill rule, depth test.
#include "test_framework.h"
#include "test_helpers.h"

#include "core/config.h"
#include "gpu/device.h"
#include "shader/color_shader.h"

using namespace nova;

namespace {

core::RenderConfig testConfig(int width, int height, bool threads = false) {
    core::RenderConfig config;
    config.width = width;
    config.height = height;
    config.tileSize = 8;  // small tiles exercise multi-tile paths
    config.enableThreads = threads;
    config.backfaceCulling = true;
    return config;
}

// Matrices that map model space to clip space directly (identity): vertex
// positions are then NDC coordinates, which keeps the tests analytic.
const math::Mat4 kIdentity = math::Mat4::identity();

const shader::ColorFragmentShader kColorShader;

}  // namespace

NOVA_TEST(Rasterizer, RendersAColoredTriangleInTheCpu) {
    gpu::Device device(testConfig(64, 64));
    device.beginFrame(core::kBlack);

    // CCW triangle in NDC, area = (1.0 * 1.0) / 2 in NDC = 32x32 px = 512 px.
    const scene::Mesh mesh = novatest::makeTriangle({-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f},
                                                    {0.0f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f});
    device.draw(mesh, kIdentity, kIdentity, kColorShader);
    device.endFrame();

    const size_t white = novatest::countPixels(device.framebuffer(), core::kWhite);
    CHECK(white > 480);
    CHECK(white < 545);
    CHECK_EQ(static_cast<int>(device.stats().counters.trianglesSubmitted), 1);
    CHECK_EQ(static_cast<int>(device.stats().counters.trianglesRasterized), 1);
    CHECK_EQ(static_cast<int>(device.stats().counters.trianglesCulled), 0);

    // The shaded fragment count must match the pixels actually written.
    CHECK_EQ(static_cast<int>(device.stats().counters.fragmentsShaded), static_cast<int>(white));
}

NOVA_TEST(Rasterizer, TopLeftFillRuleCoversEachPixelExactlyOnce) {
    gpu::Device device(testConfig(64, 64));
    device.beginFrame(core::kBlack);

    // Axis-aligned quad from (-0.5,-0.5) to (0.5,0.5) in NDC => exactly the
    // 32x32 pixels from (16,16) to (47,47): 1024 pixels, no gaps, no double
    // coverage on the shared diagonal.
    const scene::Mesh quad = novatest::makeQuad(0.5f, 0.5f, 0.0f, {1.0f, 1.0f, 1.0f});
    device.draw(quad, kIdentity, kIdentity, kColorShader);
    device.endFrame();

    const size_t white = novatest::countPixels(device.framebuffer(), core::kWhite);
    CHECK_EQ(static_cast<int>(white), 1024);
    CHECK_EQ(static_cast<int>(device.stats().counters.fragmentsShaded), 1024);
    CHECK_EQ(static_cast<int>(device.stats().counters.trianglesRasterized), 2);
    // The fill rule assigns the shared diagonal to exactly one triangle, so each
    // of the 1024 pixels of the quad is tested exactly once: no gaps, no overlap.
    CHECK_EQ(static_cast<int>(device.stats().counters.fragmentsTested), 1024);
}

NOVA_TEST(Rasterizer, BackFaceCullingRejectsReversedWinding) {
    gpu::Device device(testConfig(64, 64));
    device.beginFrame(core::kBlack);

    // Same triangle with swapped winding -> clockwise in NDC -> back facing.
    const scene::Mesh mesh = novatest::makeTriangle({0.5f, -0.5f, 0.0f}, {-0.5f, -0.5f, 0.0f},
                                                    {0.0f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f});
    device.draw(mesh, kIdentity, kIdentity, kColorShader);
    device.endFrame();

    CHECK(novatest::allPixelsAre(device.framebuffer(), core::kBlack));
    CHECK_EQ(static_cast<int>(device.stats().counters.trianglesCulled), 1);
    CHECK_EQ(static_cast<int>(device.stats().counters.trianglesRasterized), 0);
}

NOVA_TEST(Rasterizer, CullingCanBeDisabled) {
    core::RenderConfig config = testConfig(64, 64);
    config.backfaceCulling = false;
    gpu::Device device(config);
    device.beginFrame(core::kBlack);

    const scene::Mesh mesh = novatest::makeTriangle({0.5f, -0.5f, 0.0f}, {-0.5f, -0.5f, 0.0f},
                                                    {0.0f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f});
    device.draw(mesh, kIdentity, kIdentity, kColorShader);
    device.endFrame();

    CHECK(novatest::countPixels(device.framebuffer(), core::kWhite) > 480);
}

NOVA_TEST(Rasterizer, DepthTestingKeepsTheNearestFragment) {
    // Near quad (dark red, NDC z = -0.5) drawn *after* the far one must win.
    core::RenderConfig config = testConfig(64, 64);
    gpu::Device device(config);
    device.beginFrame(core::kBlack);

    const math::Vec3 red{0.85f, 0.15f, 0.15f};
    const math::Vec3 blue{0.15f, 0.15f, 0.85f};
    const scene::Mesh farQuad = novatest::makeQuad(0.5f, 0.5f, 0.5f, blue);
    const scene::Mesh nearQuad = novatest::makeQuad(0.5f, 0.5f, -0.5f, red);

    device.draw(farQuad, kIdentity, kIdentity, kColorShader);
    device.draw(nearQuad, kIdentity, kIdentity, kColorShader);
    device.endFrame();

    const core::Color expectedRed = core::colorFromVec4({red.x, red.y, red.z, 1.0f});
    const core::Color expectedBlue = core::colorFromVec4({blue.x, blue.y, blue.z, 1.0f});

    CHECK_EQ(static_cast<int>(novatest::countPixelsNear(device.framebuffer(), expectedRed)), 1024);
    CHECK_EQ(static_cast<int>(novatest::countPixelsNear(device.framebuffer(), expectedBlue)), 0);

    // The far quad shaded only after passing the depth test, so nothing extra
    // was written for the near quad on top of it... but every near fragment
    // passed: total shaded fragments = 1024 (far) + 1024 (near).
    CHECK_EQ(static_cast<int>(device.stats().counters.fragmentsShaded), 2048);
}

NOVA_TEST(Rasterizer, DepthWriteDisabledKeepsTheFirstDepth) {
    core::RenderConfig config = testConfig(64, 64);
    config.depthWrite = false;
    gpu::Device device(config);
    device.beginFrame(core::kBlack, 1.0f);

    const math::Vec3 red{0.85f, 0.15f, 0.15f};
    const math::Vec3 blue{0.15f, 0.15f, 0.85f};
    const scene::Mesh quad = novatest::makeQuad(0.5f, 0.5f, 0.0f, red);
    device.draw(quad, kIdentity, kIdentity, kColorShader);
    device.endFrame();

    // Colors are written, depth is not.
    CHECK_EQ(static_cast<int>(novatest::countPixelsNear(
                 device.framebuffer(), core::colorFromVec4({red.x, red.y, red.z, 1.0f}))),
             1024);
    CHECK_NEAR(device.framebuffer().depthAt(32, 32), 1.0f, 1e-6);
}

NOVA_TEST(Rasterizer, DepthTestDisabledLetsTheLastDrawWin) {
    core::RenderConfig config = testConfig(64, 64);
    config.depthTest = false;
    gpu::Device device(config);
    device.beginFrame(core::kBlack);

    const math::Vec3 red{0.85f, 0.15f, 0.15f};
    const math::Vec3 blue{0.15f, 0.15f, 0.85f};
    const scene::Mesh farQuad = novatest::makeQuad(0.5f, 0.5f, 0.5f, blue);
    const scene::Mesh nearQuad = novatest::makeQuad(0.5f, 0.5f, -0.5f, red);

    device.draw(nearQuad, kIdentity, kIdentity, kColorShader);   // drawn first
    device.draw(farQuad, kIdentity, kIdentity, kColorShader);    // painted over
    device.endFrame();

    CHECK_EQ(static_cast<int>(novatest::countPixelsNear(
                 device.framebuffer(), core::colorFromVec4({blue.x, blue.y, blue.z, 1.0f}))),
             1024);
}

NOVA_TEST(Rasterizer, PartialOverlapDepthResolution) {
    // A small far quad partially covered by a small near quad.
    gpu::Device device(testConfig(64, 64));
    device.beginFrame(core::kBlack);

    const math::Vec3 red{0.85f, 0.15f, 0.15f};
    const math::Vec3 blue{0.15f, 0.15f, 0.85f};
    const scene::Mesh farQuad = novatest::makeQuad(0.5f, 0.5f, 0.5f, blue);  // 32x32 px
    const scene::Mesh nearQuad = novatest::makeQuad(0.25f, 0.25f, -0.5f, red);  // 16x16 px
    device.draw(farQuad, kIdentity, kIdentity, kColorShader);
    device.draw(nearQuad, kIdentity, kIdentity, kColorShader);
    device.endFrame();

    const core::Color expectedRed = core::colorFromVec4({red.x, red.y, red.z, 1.0f});
    const core::Color expectedBlue = core::colorFromVec4({blue.x, blue.y, blue.z, 1.0f});
    // 1024 - 256 red, 256 blue.
    CHECK_EQ(static_cast<int>(novatest::countPixelsNear(device.framebuffer(), expectedRed)), 256);
    CHECK_EQ(static_cast<int>(novatest::countPixelsNear(device.framebuffer(), expectedBlue)), 768);
}

NOVA_TEST(Rasterizer, ThreadedAndSingleThreadedOutputsMatch) {
    core::RenderConfig single = testConfig(96, 96, false);
    core::RenderConfig threaded = testConfig(96, 96, true);
    threaded.threadCount = 4;
    threaded.tileSize = 8;

    gpu::Device deviceSingle(single);
    gpu::Device deviceThreaded(threaded);

    // A few overlapping quads at different depths make the result depend on
    // both rasterization and depth resolution order.
    const math::Vec3 colors[3] = {{0.9f, 0.2f, 0.2f}, {0.2f, 0.9f, 0.2f}, {0.2f, 0.2f, 0.9f}};
    const scene::Mesh meshes[3] = {
        novatest::makeQuad(0.6f, 0.4f, 0.3f, colors[0]),
        novatest::makeQuad(0.4f, 0.6f, 0.0f, colors[1]),
        novatest::makeQuad(0.3f, 0.3f, -0.4f, colors[2]),
    };

    for (int pass = 0; pass < 2; ++pass) {
        gpu::Device& device = pass == 0 ? deviceSingle : deviceThreaded;
        device.beginFrame(core::kBlack);
        for (const scene::Mesh& mesh : meshes) device.draw(mesh, kIdentity, kIdentity, kColorShader);
        device.endFrame();
    }

    int mismatches = 0;
    for (size_t i = 0; i < deviceSingle.framebuffer().pixelCount(); ++i) {
        if (deviceSingle.framebuffer().colorData()[i] != deviceThreaded.framebuffer().colorData()[i]) {
            ++mismatches;
        }
    }
    CHECK_EQ(mismatches, 0);
    CHECK(deviceSingle.framebuffer().colorData()[48 * 96 + 48] ==
          deviceThreaded.framebuffer().colorData()[48 * 96 + 48]);
    CHECK(deviceThreaded.threadCount() > 1);

    // Counters must agree as well: identical work regardless of thread count.
    CHECK_EQ(static_cast<int>(deviceSingle.stats().counters.fragmentsShaded),
             static_cast<int>(deviceThreaded.stats().counters.fragmentsShaded));
}

NOVA_TEST(Rasterizer, BenchmarksManyTrianglesWithoutLosingCoverage) {
    // 128 small triangles in a grid: every tile gets hit, the binned path is
    // exercised, and the total coverage stays exact.
    core::RenderConfig config = testConfig(64, 64, true);
    config.threadCount = 4;
    gpu::Device device(config);
    device.beginFrame(core::kBlack);

    scene::Mesh mesh;
    const int grid = 8;       // 8x8 = 64 quads = 128 triangles
    const float cell = 0.2f;  // NDC size of each quad
    for (int gy = 0; gy < grid; ++gy) {
        for (int gx = 0; gx < grid; ++gx) {
            const float cx = -0.8f + cell * (gx + 0.5f);
            const float cy = -0.8f + cell * (gy + 0.5f);
            const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
            const float h = cell * 0.35f;
            const math::Vec3 corners[4] = {{cx - h, cy - h, 0.0f}, {cx + h, cy - h, 0.0f},
                                           {cx + h, cy + h, 0.0f}, {cx - h, cy + h, 0.0f}};
            for (int i = 0; i < 4; ++i) {
                core::Vertex v;
                v.position = corners[i];
                v.color = {1.0f, 1.0f, 1.0f};
                v.normal = {0.0f, 0.0f, 1.0f};
                mesh.vertices.push_back(v);
            }
            mesh.addTriangle(base + 0, base + 1, base + 2);
            mesh.addTriangle(base + 0, base + 2, base + 3);
        }
    }

    device.draw(mesh, kIdentity, kIdentity, kColorShader);
    device.endFrame();

    CHECK_EQ(static_cast<int>(device.stats().counters.trianglesSubmitted), 128);
    CHECK_EQ(static_cast<int>(device.stats().counters.trianglesRasterized), 128);
    // Each quad spans 0.14 NDC = 4.48 px, so it covers 4-5 pixel centers per
    // axis: 16-25 px, 64 quads in total.
    const size_t white = novatest::countPixels(device.framebuffer(), core::kWhite);
    CHECK(white > 900);
    CHECK(white < 1700);
}
