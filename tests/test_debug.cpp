// NOVA GPU - Software GPU
// tests/test_debug.cpp - Phase 6: debug mode readout and frame-time history.
#include "test_framework.h"
#include "test_helpers.h"

#include "camera/camera.h"
#include "core/config.h"
#include <cstdio>
#include <string>

#include "debug/overlay.h"
#include "gpu/device.h"
#include "scene/shapes.h"
#include "shader/color_shader.h"
#include "shader/lambert_shader.h"

using namespace nova;

namespace {

core::RenderConfig testConfig(int width, int height) {
    core::RenderConfig config;
    config.width = width;
    config.height = height;
    config.tileSize = 8;
    config.enableThreads = false;
    return config;
}

// Number of pixels that differ between two framebuffers.
size_t countDifferences(const core::Framebuffer& a, const core::Framebuffer& b, int x0, int y0,
                        int x1, int y1) {
    size_t differences = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const size_t index = static_cast<size_t>(y) * a.width() + x;
            if (a.colorData()[index] != b.colorData()[index]) ++differences;
        }
    }
    return differences;
}

}  // namespace

NOVA_TEST(Debug, HudDrawsTextInsideItsPanelAndLeavesTheRestUntouched) {
    gpu::Device device(testConfig(160, 120));
    const scene::Mesh quad = novatest::makeQuad(0.5f, 0.5f, 0.0f, {0.7f, 0.7f, 0.7f});
    const shader::ColorFragmentShader shader;

    device.beginFrame(core::kBlack);
    device.draw(quad, math::Mat4::identity(), math::Mat4::identity(), shader);
    device.endFrame();

    // Snapshot the frame, then draw the HUD over it.
    std::vector<core::Color> before(device.framebuffer().colorData(),
                                    device.framebuffer().colorData() +
                                        device.framebuffer().pixelCount());
    core::Framebuffer reference(device.framebuffer().width(), device.framebuffer().height());
    std::copy(before.begin(), before.end(), reference.colorData());

    debug::Overlay overlay(device.framebuffer(), 2);
    overlay.drawHud(device.stats(), 4, 4);

    // Everything below the HUD footprint (queried from the overlay, so this test
    // does not hard-code the panel metrics) must be untouched.
    const int hudBottom = 4 + overlay.hudHeight(false);
    const size_t untouched = countDifferences(reference, device.framebuffer(), 0, hudBottom, 160, 120);
    const size_t drawn = countDifferences(reference, device.framebuffer(), 0, 0, 160, hudBottom);
    CHECK_EQ(static_cast<int>(untouched), 0);
    CHECK(drawn > 200);
}

NOVA_TEST(Debug, LowercaseTextRendersLikeUppercase) {
    // The 3x5 font has no lowercase glyphs; the overlay folds case so a
    // camel-case shader name is still readable instead of three stray capitals.
    core::Framebuffer lower(64, 16);
    core::Framebuffer upper(64, 16);
    core::Framebuffer blank(64, 16);
    const core::Color white{255, 255, 255, 255};

    debug::Overlay a(lower, 1);
    debug::Overlay b(upper, 1);
    debug::Overlay c(blank, 1);
    a.drawText(2, 2, "TransformVertexShader", white);
    b.drawText(2, 2, "TRANSFORMVERTEXSHADER", white);
    c.drawText(2, 2, ".....................", white);

    size_t different = 0;
    for (size_t i = 0; i < lower.pixelCount(); ++i) {
        if (lower.colorData()[i] != upper.colorData()[i]) ++different;
    }
    CHECK_EQ(static_cast<int>(different), 0);

    size_t drawn = 0;
    for (size_t i = 0; i < lower.pixelCount(); ++i) {
        if (lower.colorData()[i] != blank.colorData()[i]) ++drawn;
    }
    CHECK(drawn > 60);
}

NOVA_TEST(Debug, FrameTimeGraphStaysInsideItsRectangle) {
    gpu::Device device(testConfig(160, 120));
    for (int frame = 0; frame < 10; ++frame) {
        device.beginFrame(core::kBlack);
        device.endFrame();
    }
    CHECK_EQ(device.stats().frameTimes.count, 10);

    core::Framebuffer reference(device.framebuffer().width(), device.framebuffer().height());
    std::copy(device.framebuffer().colorData(),
              device.framebuffer().colorData() + device.framebuffer().pixelCount(),
              reference.colorData());

    debug::Overlay overlay(device.framebuffer(), 2);
    overlay.drawFrameTimeGraph(device.stats(), 10, 10, 60, 20);

    CHECK_EQ(static_cast<int>(countDifferences(reference, device.framebuffer(), 0, 0, 160, 9)), 0);
    CHECK_EQ(static_cast<int>(countDifferences(reference, device.framebuffer(), 0, 30, 160, 120)), 0);
    CHECK_EQ(static_cast<int>(countDifferences(reference, device.framebuffer(), 70, 9, 160, 30)), 0);
    CHECK(countDifferences(reference, device.framebuffer(), 10, 10, 70, 30) > 100);
}

NOVA_TEST(Debug, StatsTrackFrameTimesAndShaderNames) {
    gpu::Device device(testConfig(64, 64));
    const scene::Mesh quad = novatest::makeQuad(0.5f, 0.5f, 0.0f, {1.0f, 1.0f, 1.0f});
    const shader::ColorFragmentShader color;
    const shader::LambertFragmentShader lambert;

    CHECK_EQ(device.stats().frameTimes.count, 0);

    device.beginFrame(core::kBlack);
    device.draw(quad, math::Mat4::identity(), math::Mat4::identity(), color);
    device.endFrame();
    CHECK_EQ(device.stats().frameTimes.count, 1);
    CHECK(device.stats().fps > 0.0);
    CHECK(device.stats().vertexShader == "TransformVertexShader");
    CHECK(device.stats().fragmentShader == "ColorFragmentShader");

    device.beginFrame(core::kBlack);
    device.draw(quad, math::Mat4::identity(), math::Mat4::identity(), lambert);
    device.endFrame();
    CHECK_EQ(device.stats().frameTimes.count, 2);
    CHECK(device.stats().fragmentShader == "LambertFragmentShader");

    // The window reports min/avg/max over the samples it holds.
    CHECK(device.stats().frameTimes.minimum() > 0.0f);
    CHECK(device.stats().frameTimes.maximum() >= device.stats().frameTimes.minimum());
    CHECK(device.stats().frameTimes.average() > 0.0);

    device.resetStats();
    CHECK_EQ(device.stats().frameTimes.count, 0);
    CHECK(device.stats().fragmentShader == "none");
    CHECK_EQ(static_cast<int>(device.stats().counters.drawCalls), 0);
}

NOVA_TEST(Debug, MemoryReadoutSeparatesFramebufferFromWorkingSet) {
    gpu::Device device(testConfig(160, 120));
    const scene::Mesh quad = novatest::makeQuad(0.8f, 0.8f, 0.0f, {1.0f, 1.0f, 1.0f});
    const shader::ColorFragmentShader shader;
    debug::Overlay overlay(device.framebuffer(), 2);

    // Before any draw the working set is just the framebuffer.
    CHECK_EQ(static_cast<int>(device.stats().workingSetBytes),
             static_cast<int>(device.stats().framebufferBytes));

    for (int frame = 0; frame < 3; ++frame) {
        device.beginFrame(core::kBlack);
        device.draw(quad, math::Mat4::identity(), math::Mat4::identity(), shader);
        device.endFrame();
    }

    // After drawing, the rasterizer's tile bins are part of the working set, so
    // it must be strictly larger than the framebuffer alone.
    CHECK(device.stats().workingSetBytes > device.stats().framebufferBytes);

    const std::string line = overlay.memoryLine(device.stats());
    CHECK(line.find("MEM FB") != std::string::npos);
    CHECK(line.find("WORK") != std::string::npos);
    // The two figures must come from different fields - the bug this pins was
    // both halves printing the framebuffer size.
    char framebuffer[32];
    char working[32];
    std::snprintf(framebuffer, sizeof(framebuffer), "MEM FB %.2f MIB",
                  static_cast<double>(device.stats().framebufferBytes) / (1024.0 * 1024.0));
    std::snprintf(working, sizeof(working), "WORK %.2f MIB",
                  static_cast<double>(device.stats().workingSetBytes) / (1024.0 * 1024.0));
    CHECK(line.find(framebuffer) != std::string::npos);
    CHECK(line.find(working) != std::string::npos);
    CHECK(line.find(working) != line.find(framebuffer));
}

NOVA_TEST(Debug, HistoryWindowKeepsTheNewestSamples) {
    debug::FrameTimeHistory history;
    for (int i = 0; i < debug::FrameTimeHistory::kCapacity + 5; ++i) {
        history.push(static_cast<double>(i + 1));
    }
    CHECK_EQ(history.count, debug::FrameTimeHistory::kCapacity);
    // Oldest sample still held is 6 (samples 1..5 were pushed out).
    CHECK_NEAR(history.at(0), 6.0f, 1e-4);
    CHECK_NEAR(history.at(history.count - 1), static_cast<float>(debug::FrameTimeHistory::kCapacity + 5), 1e-4);
    CHECK_NEAR(history.minimum(), 6.0f, 1e-4);
    CHECK_NEAR(history.maximum(), static_cast<float>(debug::FrameTimeHistory::kCapacity + 5), 1e-4);
    history.clear();
    CHECK_EQ(history.count, 0);
    CHECK_NEAR(history.average(), 0.0, 1e-9);
}

NOVA_TEST(Debug, RendererNameIsTheCanonicalNovaString) {
    CHECK(std::string(debug::rendererName()) == "NOVA GPU \xE2\x80\x94 CPU SOFTWARE RENDERER");
    gpu::Device device(testConfig(32, 32));
    CHECK(device.stats().renderer == debug::rendererName());
    CHECK_EQ(device.stats().width, 32);
    CHECK_EQ(device.stats().height, 32);
    CHECK(device.stats().threadCount >= 1);
}
