// NOVA GPU - Software GPU
// src/core/framebuffer.h - color + depth render targets.
#pragma once

#include <cstddef>
#include <vector>

#include "core/color.h"

namespace nova::core {

// Owns the color and depth attachments of one render target.
//
// Layout: row-major, one tightly packed 32-bit Color per pixel and one 32-bit
// float depth per pixel. Index (x, y) maps to y * width + x, so scanning a row
// walks contiguous memory.
//
// Depth convention: values in [0, 1]; 0 is the near plane, 1 the far plane, and
// closer fragments have smaller values ("less" depth test).
class Framebuffer {
public:
    Framebuffer(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    size_t pixelCount() const { return static_cast<size_t>(width_) * static_cast<size_t>(height_); }

    // --- raw storage -------------------------------------------------------
    Color* colorData() { return color_.data(); }
    const Color* colorData() const { return color_.data(); }
    float* depthData() { return depth_.data(); }
    const float* depthData() const { return depth_.data(); }

    size_t colorBytes() const { return color_.size() * sizeof(Color); }
    size_t depthBytes() const { return depth_.size() * sizeof(float); }
    size_t memoryBytes() const { return colorBytes() + depthBytes(); }

    // --- clear -------------------------------------------------------------
    void clearColor(const Color& c);
    void clearDepth(float depth = 1.0f);
    void clear(const Color& c, float depth = 1.0f);

    // --- pixel access (no bounds checks: hot path) --------------------------
    size_t index(int x, int y) const { return static_cast<size_t>(y) * width_ + x; }

    Color& colorAt(int x, int y) { return color_[index(x, y)]; }
    const Color& colorAt(int x, int y) const { return color_[index(x, y)]; }
    void setColor(int x, int y, const Color& c) { color_[index(x, y)] = c; }

    float depthAt(int x, int y) const { return depth_[index(x, y)]; }
    void setDepth(int x, int y, float d) { depth_[index(x, y)] = d; }

    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width_ && y < height_;
    }

    // Depth test + write for a single fragment.
    //   depthTest = false -> always passes (no compare), write if depthWrite.
    //   depthWrite = false -> tests but does not update the depth buffer.
    bool depthTestAndWrite(int x, int y, float depth, bool depthTest, bool depthWrite) {
        float& d = depth_[index(x, y)];
        if (depthTest && !(depth < d)) return false;  // less == closer, NaN rejected
        if (depthWrite) d = depth;
        return true;
    }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Color> color_;
    std::vector<float> depth_;
};

}  // namespace nova::core
