#include "debug/overlay.h"

#include <cstdio>
#include <cstring>

#include "debug/font3x5.h"

namespace nova::debug {
namespace {

core::Color withAlpha(const core::Color& c, int alpha) {
    core::Color out = c;
    out.a = static_cast<uint8_t>(alpha);
    return out;
}

}  // namespace

Overlay::Overlay(core::Framebuffer& framebuffer, int scale)
    : framebuffer_(framebuffer), scale_(scale > 0 ? scale : 1) {}

void Overlay::setScale(int scale) { scale_ = scale > 0 ? scale : 1; }

int Overlay::lineHeight() const { return (kFontGlyphHeight + 2) * scale_; }

void Overlay::fillRect(int x, int y, int width, int height, const core::Color& color) {
    const int x0 = x < 0 ? 0 : x;
    const int y0 = y < 0 ? 0 : y;
    const int x1 = (x + width) > framebuffer_.width() ? framebuffer_.width() : (x + width);
    const int y1 = (y + height) > framebuffer_.height() ? framebuffer_.height() : (y + height);

    for (int py = y0; py < y1; ++py) {
        core::Color* row = framebuffer_.colorData() + framebuffer_.index(0, py);
        for (int px = x0; px < x1; ++px) row[px] = color;
    }
}

int Overlay::textWidth(const std::string& text) const {
    // Each glyph is 3 columns wide plus one column of spacing.
    return static_cast<int>(text.size()) * (kFontGlyphWidth + 1) * scale_;
}

int Overlay::drawText(int x, int y, const std::string& text, const core::Color& color) {
    int cursor = x;
    for (const char c : text) {
        const uint16_t glyph = glyphFor(c);
        for (int row = 0; row < kFontGlyphHeight; ++row) {
            for (int col = 0; col < kFontGlyphWidth; ++col) {
                if (((glyph >> (row * 3 + col)) & 1u) == 0u) continue;
                for (int sy = 0; sy < scale_; ++sy) {
                    for (int sx = 0; sx < scale_; ++sx) {
                        const int px = cursor + col * scale_ + sx;
                        const int py = y + row * scale_ + sy;
                        if (!framebuffer_.inBounds(px, py)) continue;
                        framebuffer_.setColor(px, py, color);
                    }
                }
            }
        }
        cursor += (kFontGlyphWidth + 1) * scale_;
    }
    return textWidth(text);
}

int Overlay::drawTextShadowed(int x, int y, const std::string& text, const core::Color& color,
                              const core::Color& shadow) {
    drawText(x + scale_, y + scale_, text, shadow);
    return drawText(x, y, text, color);
}

void Overlay::drawHud(const RenderStats& stats, int x, int y) {
    char line[160];

    // Renderer name, ASCII-folded for the 3x5 bitmap font (the em dash becomes a
    // hyphen; the console output keeps the real character).
    std::string renderer = stats.renderer;
    for (char& c : renderer) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 32 || u > 126) c = '-';
    }

    const int panelWidth = textWidth("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX") + 8 * scale_;
    const int panelHeight = lineHeight() * 5 + 6 * scale_;
    fillRect(x - 4 * scale_, y - 4 * scale_, panelWidth, panelHeight,
             withAlpha(core::kBlack, 255));

    const core::Color fg = core::kWhite;
    const core::Color accent = {120, 220, 255, 255};
    const core::Color shadow = {0, 0, 0, 255};

    int cursorY = y;
    drawTextShadowed(x, cursorY, renderer, accent, shadow);
    cursorY += lineHeight();

    std::snprintf(line, sizeof(line), "FPS %.1f  FRAME %.2f MS", stats.fps,
                  stats.timings.frameMs);
    drawTextShadowed(x, cursorY, line, fg, shadow);
    cursorY += lineHeight();

    std::snprintf(line, sizeof(line), "TRIS %llu  RAST %llu  CULL %llu",
                  static_cast<unsigned long long>(stats.counters.trianglesSubmitted),
                  static_cast<unsigned long long>(stats.counters.trianglesRasterized),
                  static_cast<unsigned long long>(stats.counters.trianglesCulled));
    drawTextShadowed(x, cursorY, line, fg, shadow);
    cursorY += lineHeight();

    std::snprintf(line, sizeof(line), "FRAG %llu  SHADED %llu",
                  static_cast<unsigned long long>(stats.counters.fragmentsTested),
                  static_cast<unsigned long long>(stats.counters.fragmentsShaded));
    drawTextShadowed(x, cursorY, line, fg, shadow);
    cursorY += lineHeight();

    std::snprintf(line, sizeof(line), "THREADS %d  RES %dx%d", stats.threadCount, stats.width,
                  stats.height);
    drawTextShadowed(x, cursorY, line, fg, shadow);
}

}  // namespace nova::debug
