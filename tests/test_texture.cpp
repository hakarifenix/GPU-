// NOVA GPU - Software GPU
// tests/test_texture.cpp - Phase 3: texture storage, filtering, image loading.
#include "test_framework.h"
#include "test_helpers.h"

#include <cstdio>
#include <string>
#include <vector>

#include "core/config.h"
#include "gpu/device.h"
#include "scene/shapes.h"
#include "shader/texture_shader.h"
#include "texture/image_io.h"
#include "texture/texture.h"

using namespace nova;

namespace {

// 2x2 texture with four distinct corners:
//   (0,0) red    (1,0) green
//   (0,1) blue   (1,1) yellow
texture::Texture make2x2() {
    std::vector<core::Color> pixels = {
        core::Color{255, 0, 0, 255},
        core::Color{0, 255, 0, 255},
        core::Color{0, 0, 255, 255},
        core::Color{255, 255, 0, 255},
    };
    return texture::Texture(2, 2, std::move(pixels));
}

bool near(const math::Vec4& a, const math::Vec4& b, float tolerance = 1.0f / 255.0f) {
    return std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance &&
           std::fabs(a.z - b.z) <= tolerance;
}

core::RenderConfig testConfig(int width, int height) {
    core::RenderConfig config;
    config.width = width;
    config.height = height;
    config.tileSize = 8;
    config.enableThreads = false;
    return config;
}

}  // namespace

NOVA_TEST(Texture, StorageAndTexelAccess) {
    const texture::Texture t = make2x2();
    CHECK_EQ(t.width(), 2);
    CHECK_EQ(t.height(), 2);
    CHECK_EQ(static_cast<int>(t.memoryBytes()), 4 * 4);
    CHECK(t.texel(0, 0) == core::kRed);
    CHECK(t.texel(1, 0) == core::kGreen);
    CHECK(t.texel(0, 1) == core::kBlue);
    CHECK(t.texel(1, 1) == core::kYellow);

    // Texel access wraps instead of reading out of bounds.
    CHECK(t.texel(2, 0) == core::kRed);
    CHECK(t.texel(-1, 0) == core::kGreen);
    CHECK(t.texel(0, 2, texture::WrapMode::Clamp) == core::kBlue);
}

NOVA_TEST(Texture, NearestSamplesExactTexels) {
    const texture::Texture t = make2x2();
    texture::Sampler sampler;
    sampler.filter = texture::SampleMode::Nearest;

    // Texel centers in uv space are (2x+1)/4.
    CHECK(near(t.sample({0.25f, 0.25f}, sampler), core::colorToVec4(core::kRed)));
    CHECK(near(t.sample({0.75f, 0.25f}, sampler), core::colorToVec4(core::kGreen)));
    CHECK(near(t.sample({0.25f, 0.75f}, sampler), core::colorToVec4(core::kBlue)));
    CHECK(near(t.sample({0.75f, 0.75f}, sampler), core::colorToVec4(core::kYellow)));
}

NOVA_TEST(Texture, BilinearIsExactAtTexelCenters) {
    const texture::Texture t = make2x2();
    texture::Sampler sampler;
    sampler.filter = texture::SampleMode::Bilinear;

    CHECK(near(t.sample({0.25f, 0.25f}, sampler), core::colorToVec4(core::kRed)));
    CHECK(near(t.sample({0.75f, 0.25f}, sampler), core::colorToVec4(core::kGreen)));
    CHECK(near(t.sample({0.25f, 0.75f}, sampler), core::colorToVec4(core::kBlue)));
    CHECK(near(t.sample({0.75f, 0.75f}, sampler), core::colorToVec4(core::kYellow)));
}

NOVA_TEST(Texture, BilinearBlendsTheFourNearestTexels) {
    const texture::Texture t = make2x2();
    texture::Sampler sampler;
    sampler.filter = texture::SampleMode::Bilinear;

    // Exactly between all four texels (uv 0.5, 0.5) the blend is their mean.
    const math::Vec4 s = t.sample({0.5f, 0.5f}, sampler);
    CHECK_NEAR(s.x, (255.0f + 0.0f + 0.0f + 255.0f) / 4.0f / 255.0f, 1e-3);
    CHECK_NEAR(s.y, (0.0f + 255.0f + 0.0f + 255.0f) / 4.0f / 255.0f, 1e-3);
    CHECK_NEAR(s.z, (0.0f + 0.0f + 255.0f + 0.0f) / 4.0f / 255.0f, 1e-3);

    // Halfway along u at v = 0 is the mean of red and green.
    const math::Vec4 edge = t.sample({0.5f, 0.25f}, sampler);
    CHECK_NEAR(edge.x, 0.5f, 1e-3);
    CHECK_NEAR(edge.y, 0.5f, 1e-3);
    CHECK_NEAR(edge.z, 0.0f, 1e-3);
}

NOVA_TEST(Texture, WrapModesDifferOutsideTheUnitSquare) {
    const texture::Texture t = make2x2();

    texture::Sampler repeat;
    repeat.filter = texture::SampleMode::Nearest;
    repeat.wrap = texture::WrapMode::Repeat;
    // u = 1.25 wraps back to the left half, v = 0.25 stays on the first row.
    CHECK(near(t.sample({1.25f, 0.25f}, repeat), core::colorToVec4(core::kRed)));

    texture::Sampler clamp;
    clamp.filter = texture::SampleMode::Nearest;
    clamp.wrap = texture::WrapMode::Clamp;
    // Clamping pins u to the right edge instead.
    CHECK(near(t.sample({1.25f, 0.25f}, clamp), core::colorToVec4(core::kGreen)));

    // Repeat also tiles with bilinear filtering.
    texture::Sampler bilinear = repeat;
    bilinear.filter = texture::SampleMode::Bilinear;
    CHECK(near(t.sample({1.5f, 0.25f}, bilinear), math::Vec4{0.5f, 0.5f, 0.0f, 1.0f}, 1e-3));
}

NOVA_TEST(Texture, EmptyTextureSamplesAsMagenta) {
    const texture::Texture empty;
    CHECK(empty.empty());
    CHECK_EQ(empty.width(), 0);
    const math::Vec4 s = empty.sample({0.5f, 0.5f});
    CHECK(near(s, {1.0f, 0.0f, 1.0f, 1.0f}));
    CHECK_EQ(static_cast<int>(empty.memoryBytes()), 0);
}

NOVA_TEST(Texture, ProceduralConstructors) {
    const texture::Texture checker = texture::Texture::checker(8, 8, 2, core::kWhite, core::kBlack);
    CHECK_EQ(checker.width(), 8);
    CHECK(checker.texel(0, 0) == core::kWhite);
    CHECK(checker.texel(5, 0) == core::kBlack);
    CHECK(checker.texel(5, 5) == core::kWhite);

    const texture::Texture solid = texture::Texture::solid(4, 4, core::kBlue);
    CHECK(solid.texel(3, 3) == core::kBlue);

    const texture::Texture grid = texture::Texture::uvGrid(16, 16, 4);
    CHECK_EQ(grid.width(), 16);
    CHECK(grid.texel(0, 0) == core::kWhite);  // grid line
}

NOVA_TEST(Texture, PpmRoundTripPreservesPixels) {
    // Build a non-trivial texture, write it, read it back and compare.
    std::vector<core::Color> pixels;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            pixels.push_back(core::Color{static_cast<uint8_t>(x * 60),
                                         static_cast<uint8_t>(y * 80),
                                         static_cast<uint8_t>(x * y * 20), 255});
        }
    }
    const texture::Texture original(4, 3, pixels);

    // In-memory encode/decode must round-trip too.
    const std::string encoded = texture::encodePpm(original);
    const texture::Texture decoded = texture::decodePpm(
        reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size());
    CHECK_EQ(decoded.width(), 4);
    CHECK_EQ(decoded.height(), 3);
    for (size_t i = 0; i < original.pixels().size(); ++i) {
        CHECK(decoded.pixels()[i] == original.pixels()[i]);
    }

    // And through the filesystem.
    const std::string path = "nova_test_texture_roundtrip.ppm";
    CHECK(texture::savePpm(path, original));
    const texture::Texture loaded = texture::loadPpm(path);
    std::remove(path.c_str());
    CHECK_EQ(loaded.width(), 4);
    CHECK_EQ(loaded.height(), 3);
    for (size_t i = 0; i < original.pixels().size(); ++i) {
        CHECK(loaded.pixels()[i] == original.pixels()[i]);
    }
}

NOVA_TEST(Texture, PpmDecoderHandlesAsciiAndRejectsGarbage) {
    // P3 (ASCII) with comments in the header.
    const std::string ascii = "P3\n# a comment\n2 1\n255\n255 0 0  0 128 255\n";
    const texture::Texture t = texture::decodePpm(
        reinterpret_cast<const uint8_t*>(ascii.data()), ascii.size());
    CHECK_EQ(t.width(), 2);
    CHECK_EQ(t.height(), 1);
    CHECK(t.texel(0, 0) == core::kRed);
    CHECK_EQ(static_cast<int>(t.texel(1, 0).g), 128);
    CHECK_EQ(static_cast<int>(t.texel(1, 0).b), 255);

    std::string error;
    const std::string junk = "not an image";
    CHECK(texture::decodePpm(reinterpret_cast<const uint8_t*>(junk.data()), junk.size(), &error)
              .empty());
    CHECK(!error.empty());

    // Truncated binary payload must be rejected rather than read out of bounds.
    const std::string truncated = "P6\n4 4\n255\nabcdef";
    CHECK(texture::decodePpm(reinterpret_cast<const uint8_t*>(truncated.data()),
                             truncated.size())
              .empty());

    CHECK(texture::loadPpm("definitely_missing_file.ppm").empty());
}

NOVA_TEST(Texture, TexturedQuadRendersEachTexelInItsQuadrant) {
    // A quad spanning NDC [-0.5, 0.5] covers screen pixels 16..47 on a 64x64
    // buffer, with uv (0,0) at the bottom-left corner (NDC y is up, screen y is
    // down). Each 2x2 texel therefore owns one 16x16 quadrant.
    const texture::Texture t = make2x2();
    texture::Sampler sampler;
    sampler.filter = texture::SampleMode::Nearest;
    sampler.wrap = texture::WrapMode::Clamp;
    shader::TexturedFragmentShader fragment(t, sampler);
    fragment.setModulateByVertexColor(true);

    const scene::Mesh quad = scene::makeQuad(1.0f, 1.0f, {1.0f, 1.0f, 1.0f});
    gpu::Device device(testConfig(64, 64));
    device.beginFrame(core::kBlack);
    device.draw(quad, math::Mat4::identity(), math::Mat4::identity(), fragment);
    device.endFrame();

    const core::Framebuffer& fb = device.framebuffer();
    CHECK_EQ(static_cast<int>(novatest::countPixels(fb, core::kBlack)), 64 * 64 - 1024);
    // Bottom-left quadrant (screen bottom) is uv (0,0) = red ...
    CHECK(fb.colorData()[44 * 64 + 20] == core::kRed);
    // ... bottom-right is uv (1,0) = green, top-left uv (0,1) = blue and
    // top-right uv (1,1) = yellow.
    CHECK(fb.colorData()[44 * 64 + 40] == core::kGreen);
    CHECK(fb.colorData()[20 * 64 + 20] == core::kBlue);
    CHECK(fb.colorData()[20 * 64 + 40] == core::kYellow);
}

NOVA_TEST(Texture, BilinearSoftensTheTexelBoundary) {
    // With a 2x2 texture the boundary between texels is a hard edge under nearest
    // filtering and a blend under bilinear filtering.
    const texture::Texture t = make2x2();
    const scene::Mesh quad = scene::makeQuad(1.0f, 1.0f, {1.0f, 1.0f, 1.0f});

    size_t blended = 0;
    for (int mode = 0; mode < 2; ++mode) {
        texture::Sampler sampler;
        sampler.filter = mode == 0 ? texture::SampleMode::Nearest
                                   : texture::SampleMode::Bilinear;
        sampler.wrap = texture::WrapMode::Clamp;
        shader::TexturedFragmentShader fragment(t, sampler);

        gpu::Device device(testConfig(64, 64));
        device.beginFrame(core::kBlack);
        device.draw(quad, math::Mat4::identity(), math::Mat4::identity(), fragment);
        device.endFrame();

        // Count quad pixels that are none of the four exact texel colors.
        size_t mixed = 0;
        for (size_t i = 0; i < device.framebuffer().pixelCount(); ++i) {
            const core::Color c = device.framebuffer().colorData()[i];
            const bool exact = c == core::kRed || c == core::kGreen || c == core::kBlue ||
                               c == core::kYellow || c == core::kBlack;
            if (!exact) ++mixed;
        }
        if (mode == 0) {
            CHECK_EQ(static_cast<int>(mixed), 0);  // nearest: blocks only
        } else {
            blended = mixed;
        }
    }
    // Bilinear blends along the texel boundary: the middle row/column of the quad.
    CHECK(blended > 0);
}
