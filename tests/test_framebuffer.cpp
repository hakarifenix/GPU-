// NOVA GPU - Software GPU
// tests/test_framebuffer.cpp - color/depth storage and the depth test.
#include "test_framework.h"

#include "core/color.h"
#include "core/framebuffer.h"

using nova::core::Color;
using nova::core::Framebuffer;
using nova::core::colorFromFloat;

NOVA_TEST(Framebuffer, SizeAndIndexing) {
    Framebuffer fb(8, 4);
    CHECK_EQ(fb.width(), 8);
    CHECK_EQ(fb.height(), 4);
    CHECK_EQ(static_cast<int>(fb.pixelCount()), 32);
    CHECK_EQ(static_cast<int>(fb.index(3, 2)), 19);  // y * width + x
    CHECK_EQ(static_cast<int>(fb.colorBytes()), 32 * 4);
    CHECK_EQ(static_cast<int>(fb.depthBytes()), 32 * 4);
    CHECK(fb.inBounds(0, 0));
    CHECK(fb.inBounds(7, 3));
    CHECK(!fb.inBounds(8, 3));
    CHECK(!fb.inBounds(0, -1));
}

NOVA_TEST(Framebuffer, Clear) {
    Framebuffer fb(4, 4);
    fb.clear(nova::core::kRed, 0.5f);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            CHECK(fb.colorAt(x, y) == nova::core::kRed);
            CHECK_NEAR(fb.depthAt(x, y), 0.5f, 1e-6);
        }
    }
    fb.clearColor(nova::core::kBlue);
    CHECK(fb.colorAt(1, 1) == nova::core::kBlue);
}

NOVA_TEST(Framebuffer, DepthTestWriteCombinations) {
    // (test, write) -> expected pass/write behaviour for a fragment at depth 0.5
    // over a buffer cleared to 1.0.
    struct Case {
        bool test;
        bool write;
        bool expectPass;
        float expectStored;
    };
    const Case cases[4] = {
        {true, true, true, 0.5f},     // normal: closer wins, depth updated
        {true, false, true, 1.0f},    // test only: depth untouched
        {false, true, true, 0.5f},    // no test: fragment always written
        {false, false, true, 1.0f},   // nothing at all
    };

    for (const Case& c : cases) {
        Framebuffer fb(2, 2);
        fb.clear(nova::core::kBlack, 1.0f);
        CHECK(fb.depthTestAndWrite(1, 1, 0.5f, c.test, c.write) == c.expectPass);
        CHECK_NEAR(fb.depthAt(1, 1), c.expectStored, 1e-6);
    }

    // A farther fragment fails the "less" test and must not modify depth.
    Framebuffer fb(2, 2);
    fb.clear(nova::core::kBlack, 0.25f);
    CHECK(!fb.depthTestAndWrite(0, 0, 0.75f, true, true));
    CHECK_NEAR(fb.depthAt(0, 0), 0.25f, 1e-6);

    // Equal depth is rejected too (strictly less keeps the first draw).
    CHECK(!fb.depthTestAndWrite(0, 0, 0.25f, true, true));
}

NOVA_TEST(Color, FloatToByteClamping) {
    CHECK_EQ(colorFromFloat(0.0f, 0.5f, 1.0f).r, 0);
    CHECK_EQ(colorFromFloat(0.0f, 0.5f, 1.0f).g, 128);  // 0.5 * 255 + 0.5 = 128
    CHECK_EQ(colorFromFloat(0.0f, 0.5f, 1.0f).b, 255);

    // Out-of-range values clamp instead of wrapping.
    const Color clamped = colorFromFloat(-1.0f, 2.0f, 0.5f);
    CHECK_EQ(clamped.r, 0);
    CHECK_EQ(clamped.g, 255);

    // Round trip.
    const Color c = colorFromFloat(0.25f, 0.5f, 0.75f);
    const nova::math::Vec4 v = nova::core::colorToVec4(c);
    CHECK_NEAR(v.x, 0.25f, 1.0f / 255.0f);
    CHECK_NEAR(v.z, 0.75f, 1.0f / 255.0f);
}
