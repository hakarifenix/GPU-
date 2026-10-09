// NOVA GPU - Software GPU
// tests/test_clipping.cpp - near-plane clipping (unit + end to end).
#include "test_framework.h"
#include "test_helpers.h"

#include "core/config.h"
#include "gpu/device.h"
#include "pipeline/clipper.h"
#include "math/math_utils.h"
#include "shader/color_shader.h"

using namespace nova;
using namespace nova::math;
using namespace nova::pipeline;

namespace {

ClipVertex clipVertex(float x, float y, float z, float w) {
    ClipVertex v;
    v.clip = {x, y, z, w};
    v.varying.worldPosition = {x, y, z};
    v.varying.normal = {0.0f, 0.0f, 1.0f};
    v.varying.color = {1.0f, 1.0f, 1.0f};
    v.varying.uv = {0.0f, 0.0f};
    return v;
}

// Signed distance to the near plane z + w >= 0.
float nearDistance(const ClipVertex& v) { return v.clip.z + v.clip.w; }

}  // namespace

NOVA_TEST(Clipping, FullyInsideTriangleIsUnchanged) {
    const ClipVertex in[3] = {clipVertex(-0.5f, -0.5f, 0.0f, 1.0f), clipVertex(0.5f, -0.5f, 0.0f, 1.0f),
                              clipVertex(0.0f, 0.5f, 0.0f, 1.0f)};
    ClipVertex out[4];
    const int count = clipTriangle(in, true, out);
    CHECK_EQ(count, 3);
    for (int i = 0; i < 3; ++i) {
        CHECK_NEAR(out[i].clip.x, in[i].clip.x, 1e-6);
        CHECK_NEAR(out[i].clip.y, in[i].clip.y, 1e-6);
        CHECK_NEAR(out[i].clip.w, in[i].clip.w, 1e-6);
    }
}

NOVA_TEST(Clipping, FullyOutsideTriangleIsDiscarded) {
    // Every vertex behind the camera (w < 0) and behind the near plane.
    const ClipVertex in[3] = {clipVertex(-0.5f, -0.5f, 0.0f, -1.0f), clipVertex(0.5f, -0.5f, 0.0f, -1.0f),
                              clipVertex(0.0f, 0.5f, 0.0f, -2.0f)};
    ClipVertex out[4];
    CHECK_EQ(clipTriangle(in, true, out), 0);
}

NOVA_TEST(Clipping, PartiallyClippedTriangleBecomesAQuad) {
    // Two vertices in front (z + w = +0.5), one strictly behind (z + w = -1).
    // Clipping such a triangle yields a quad.
    const ClipVertex in[3] = {clipVertex(0.0f, 0.0f, -0.5f, 1.0f), clipVertex(1.0f, 0.0f, -0.5f, 1.0f),
                              clipVertex(-1.0f, 0.0f, -2.0f, 1.0f)};
    ClipVertex out[4];
    const int count = clipTriangle(in, true, out);
    CHECK_EQ(count, 4);

    // Every emitted vertex must sit on or in front of the near plane.
    for (int i = 0; i < count; ++i) {
        CHECK(nearDistance(out[i]) >= -1e-5f);
    }
    // At least one vertex is the original front vertex, and at least two are
    // intersections exactly on the plane.
    int onPlane = 0;
    for (int i = 0; i < count; ++i) {
        if (std::fabs(nearDistance(out[i])) < 1e-4f) ++onPlane;
    }
    CHECK_EQ(onPlane, 2);
}

NOVA_TEST(Clipping, InterpolatedAttributesAreBlended) {
    // Plane distances: a = +0.5, b = c = -1 -> t = 0.5 / (0.5 + 1) = 1/3 for the
    // a -> b edge and t = 2/3 for the c -> a edge.
    ClipVertex a = clipVertex(0.0f, 0.0f, -0.5f, 1.0f);
    a.varying.color = {1.0f, 0.0f, 0.0f};
    ClipVertex b = clipVertex(1.0f, 0.0f, -2.0f, 1.0f);
    b.varying.color = {0.0f, 0.0f, 1.0f};
    ClipVertex c = clipVertex(-1.0f, 0.0f, -2.0f, 1.0f);
    c.varying.color = {0.0f, 0.0f, 1.0f};

    const ClipVertex in[3] = {a, b, c};
    ClipVertex out[4];
    // One vertex in front, two behind -> a triangle with two new vertices.
    const int count = clipTriangle(in, true, out);
    CHECK_EQ(count, 3);

    // Both intersection vertices sit one third of the way from a (red) to the
    // blue back vertices, so their color is 2/3 red + 1/3 blue.
    for (int i = 1; i < count; ++i) {
        CHECK_NEAR(out[i].varying.color.x, 2.0f / 3.0f, 1e-4);
        CHECK_NEAR(out[i].varying.color.z, 1.0f / 3.0f, 1e-4);
    }
    // The original front vertex is preserved exactly.
    CHECK_NEAR(out[0].varying.color.x, 1.0f, 1e-6);
}

NOVA_TEST(Clipping, PolygonClipAgainstASinglePlane) {
    // Clip a square against x >= 0 in 2D (z = 0, w = 1 planes).
    const ClipVertex in[4] = {clipVertex(-1.0f, -1.0f, 0.0f, 1.0f), clipVertex(1.0f, -1.0f, 0.0f, 1.0f),
                              clipVertex(1.0f, 1.0f, 0.0f, 1.0f), clipVertex(-1.0f, 1.0f, 0.0f, 1.0f)};
    ClipVertex out[8];
    const Vec4 planeX{1.0f, 0.0f, 0.0f, 0.0f};  // x >= 0
    const int count = clipPolygonAgainstPlane(in, 4, planeX, out, 8);
    CHECK_EQ(count, 4);
    for (int i = 0; i < count; ++i) {
        CHECK(out[i].clip.x >= -1e-5f);
    }
    // Two of the four vertices are the intersections at x = 0.
    int atZero = 0;
    for (int i = 0; i < count; ++i) {
        if (std::fabs(out[i].clip.x) < 1e-4f) ++atZero;
    }
    CHECK_EQ(atZero, 2);
}

NOVA_TEST(Clipping, EndToEndNearPlaneClipRendersWithoutNaN) {
    // A huge quad that straddles the camera plane. Clipping must run before the
    // perspective divide, otherwise w <= 0 would produce garbage coordinates.
    core::RenderConfig config;
    config.width = 64;
    config.height = 64;
    config.tileSize = 8;
    config.enableThreads = false;
    config.clipNearPlane = true;

    gpu::Device device(config);
    device.beginFrame(core::kBlack);

    // World-space quad spanning z from +2 (behind the camera at z = 0 looking
    // towards -Z) to -6, x from -6 to 6.
    scene::Mesh mesh;
    const Vec3 corners[4] = {{-6.0f, -3.0f, -6.0f}, {6.0f, -3.0f, -6.0f}, {6.0f, 3.0f, 2.0f}, {-6.0f, 3.0f, 2.0f}};
    for (int i = 0; i < 4; ++i) {
        core::Vertex v;
        v.position = corners[i];
        v.color = {1.0f, 1.0f, 1.0f};
        v.normal = {0.0f, 0.0f, 1.0f};
        mesh.vertices.push_back(v);
    }
    // Counter-clockwise as seen from the camera (+Z side), so the front faces
    // survive back-face culling.
    mesh.addTriangle(0, 1, 2);
    mesh.addTriangle(0, 2, 3);

    const Mat4 view = lookAt({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f});
    const Mat4 projection = perspective(radians(60.0f), 1.0f, 0.1f, 100.0f);

    const shader::ColorFragmentShader shader;
    device.draw(mesh, Mat4::identity(), projection * view, shader);
    device.endFrame();

    // Something must be drawn, no depth may be NaN, and every depth stays in range.
    CHECK(novatest::countPixels(device.framebuffer(), core::kWhite) > 0);
    for (size_t i = 0; i < device.framebuffer().pixelCount(); ++i) {
        const float d = device.framebuffer().depthData()[i];
        CHECK(!(d != d));  // NaN check
        CHECK(d >= 0.0f && d <= 1.0f);
    }
    CHECK(device.stats().counters.trianglesClipped > 0);
}

NOVA_TEST(Clipping, VisibleGeometrySkipsTheClipper) {
    // Sanity check for the fast path: fully visible geometry reports no clipping.
    core::RenderConfig config;
    config.width = 64;
    config.height = 64;
    gpu::Device device(config);
    device.beginFrame(core::kBlack);

    const scene::Mesh quad = novatest::makeQuad(0.5f, 0.5f, 0.0f, {1.0f, 1.0f, 1.0f});
    const shader::ColorFragmentShader shader;
    device.draw(quad, Mat4::identity(), Mat4::identity(), shader);
    device.endFrame();

    CHECK_EQ(static_cast<int>(device.stats().counters.trianglesClipped), 0);
    CHECK_EQ(static_cast<int>(novatest::countPixels(device.framebuffer(), core::kWhite)), 1024);
}
