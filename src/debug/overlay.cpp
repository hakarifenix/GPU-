#include "debug/overlay.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

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
    for (char c : text) {
        // The generated font is uppercase-only, so lowercase is folded to its
        // uppercase glyph - otherwise camel-case names such as
        // "TransformVertexShader" would render as just "T V S".
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - ('a' - 'A'));
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

namespace {
// Lines drawn by drawHud: renderer, FPS/frame, triangles, culling, fragments,
// threads (+ shader and memory in verbose mode).
constexpr int kCompactLines = 6;
constexpr int kVerboseLines = 8;
constexpr int kGraphHeight = 24;  // in pixels, before scaling
}  // namespace

int Overlay::hudWidth() const {
    return textWidth("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX") + 12 * scale_;
}

int Overlay::hudHeight(bool verbose) const {
    const int lines = verbose ? kVerboseLines : kCompactLines;
    const int graph = verbose ? (kGraphHeight * scale_ + 4 * scale_) : 0;
    return lineHeight() * lines + graph + 12 * scale_;
}

void Overlay::drawFrameTimeGraph(const RenderStats& stats, int x, int y, int width, int height) {
    const core::Color background{0, 0, 0, 255};
    const core::Color grid{60, 70, 90, 255};
    const core::Color bar{120, 220, 255, 255};
    const core::Color averageLine{255, 190, 90, 255};

    fillRect(x, y, width, height, background);
    for (int gy = y; gy < y + height; gy += 4 * scale_) {
        fillRect(x, gy, width, scale_ > 1 ? 1 : 1, grid);
    }

    const int samples = stats.frameTimes.count;
    if (samples <= 0 || height <= 0) return;

    const float maximum = stats.frameTimes.maximum();
    if (maximum <= 0.0f) return;

    const int bars = std::min(samples, width);
    for (int i = 0; i < bars; ++i) {
        const float value = stats.frameTimes.at(samples - bars + i);
        int barHeight = static_cast<int>(value / maximum * static_cast<float>(height) + 0.5f);
        if (barHeight < 1) barHeight = 1;
        if (barHeight > height) barHeight = height;
        fillRect(x + i, y + height - barHeight, 1, barHeight, bar);
    }

    const double average = stats.frameTimes.average();
    const int averageY =
        y + height - static_cast<int>(average / static_cast<double>(maximum) * height);
    if (averageY >= y && averageY < y + height) {
        fillRect(x, averageY, width, scale_ > 1 ? scale_ : 1, averageLine);
    }
}

std::string Overlay::memoryLine(const RenderStats& stats) const {
    char line[128];
    std::snprintf(line, sizeof(line), "MEM FB %.2f MIB  WORK %.2f MIB",
                  static_cast<double>(stats.framebufferBytes) / (1024.0 * 1024.0),
                  static_cast<double>(stats.workingSetBytes) / (1024.0 * 1024.0));
    return std::string(line);
}

void Overlay::drawHud(const RenderStats& stats, int x, int y, bool verbose) {
    char line[200];

    // Renderer name, ASCII-folded for the 3x5 bitmap font (the em dash becomes a
    // hyphen; the console output keeps the real character).
    std::string renderer = stats.renderer;
    for (char& c : renderer) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 32 || u > 126) c = '-';
    }

    const int graphHeight = kGraphHeight * scale_;
    const int textLines = verbose ? kVerboseLines : kCompactLines;
    const int panelWidth = hudWidth() - 4 * scale_;
    const int panelHeight = lineHeight() * textLines + 6 * scale_ +
                            (verbose ? graphHeight + 4 * scale_ : 0);
    fillRect(x - 4 * scale_, y - 4 * scale_, panelWidth, panelHeight,
             withAlpha(core::kBlack, 255));

    const core::Color fg = core::kWhite;
    const core::Color accent = {120, 220, 255, 255};
    const core::Color shadow = {0, 0, 0, 255};

    int cursorY = y;
    drawTextShadowed(x, cursorY, renderer, accent, shadow);
    cursorY += lineHeight();

    // FPS and frame time, with the min/max of the recent frame-time window.
    if (stats.frameTimes.count > 0) {
        std::snprintf(line, sizeof(line), "FPS %.1f  FRAME %.2f MS  MIN %.2f  MAX %.2f", stats.fps,
                      stats.timings.frameMs, static_cast<double>(stats.frameTimes.minimum()),
                      static_cast<double>(stats.frameTimes.maximum()));
    } else {
        std::snprintf(line, sizeof(line), "FPS %.1f  FRAME %.2f MS", stats.fps,
                      stats.timings.frameMs);
    }
    drawTextShadowed(x, cursorY, line, fg, shadow);
    cursorY += lineHeight();

    // Triangles: submitted by the application, visible (rasterized) and culled.
    std::snprintf(line, sizeof(line), "TRIS %llu  VISIBLE %llu  CULLED %llu",
                  static_cast<unsigned long long>(stats.counters.trianglesSubmitted),
                  static_cast<unsigned long long>(stats.visibleTriangles()),
                  static_cast<unsigned long long>(stats.counters.trianglesCulled +
                                                  stats.counters.trianglesFrustumCulled));
    drawTextShadowed(x, cursorY, line, fg, shadow);
    cursorY += lineHeight();

    std::snprintf(line, sizeof(line), "FRUSTUM CULLED %llu  OBJECTS %llu/%llu",
                  static_cast<unsigned long long>(stats.counters.trianglesFrustumCulled),
                  static_cast<unsigned long long>(stats.counters.objectsCulled),
                  static_cast<unsigned long long>(stats.counters.objectsSubmitted));
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
    cursorY += lineHeight();

    if (verbose) {
        std::snprintf(line, sizeof(line), "SHADER %s + %s", stats.vertexShader.c_str(),
                      stats.fragmentShader.c_str());
        drawTextShadowed(x, cursorY, line, fg, shadow);
        cursorY += lineHeight();

        drawTextShadowed(x, cursorY, memoryLine(stats), fg, shadow);
        cursorY += lineHeight();

        drawFrameTimeGraph(stats, x, cursorY, panelWidth - 8 * scale_, graphHeight);
        cursorY += graphHeight;
    }
    (void)cursorY;
}

}  // namespace nova::debug
