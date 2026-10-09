// NOVA GPU - Software GPU
// src/debug/overlay.h - debug HUD drawn into the framebuffer by the renderer.
//
// The overlay is plain framebuffer drawing (no API, no GPU): it proves the
// render target is a normal CPU image and keeps the debug mode visible in
// headless output such as the PPM files written by the examples.
#pragma once

#include <string>

#include "core/color.h"
#include "core/framebuffer.h"
#include "debug/stats.h"

namespace nova::debug {

class Overlay {
public:
    Overlay(core::Framebuffer& framebuffer, int scale = 2);

    void setScale(int scale);

    // Rectangle fill, clipped to the framebuffer.
    void fillRect(int x, int y, int width, int height, const core::Color& color);

    // Draws ASCII text with the 3x5 font. Unknown/non-ASCII characters are
    // skipped (glyph 0 is blank). Returns the width in pixels consumed.
    int drawText(int x, int y, const std::string& text, const core::Color& color);

    // Text with a 1-pixel (scaled) drop shadow for contrast on light scenes.
    int drawTextShadowed(int x, int y, const std::string& text, const core::Color& color,
                         const core::Color& shadow);

    // Standard debug mode readout: renderer name, FPS, frame time, triangle
    // counts, thread count and resolution.
    void drawHud(const RenderStats& stats, int x = 8, int y = 8);

    int textWidth(const std::string& text) const;
    int lineHeight() const;

private:
    core::Framebuffer& framebuffer_;
    int scale_ = 2;
};

}  // namespace nova::debug
