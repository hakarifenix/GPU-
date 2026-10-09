// NOVA GPU - Software GPU
// src/texture/texture.h - CPU texture storage and sampling.
//
// A texture is a plain RGBA8 image plus a sampler state. Sampling lives in the
// fragment stage: the rasterizer only interpolates uv, and a fragment shader
// decides what to fetch from it. Keeping sampling out of the rasterizer means
// the same texture object works for every shader and every filter mode.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/color.h"
#include "math/vec.h"

namespace nova::texture {

// Texel selection when the uv coordinate falls between texel centers.
enum class SampleMode {
    Nearest,   // cheapest; visible texel blocks
    Bilinear,  // 4-tap blend; smoother, still a handful of multiplies per fragment
};

// What happens to uv outside [0, 1].
enum class WrapMode {
    Repeat,  // wrap around (tiling)
    Clamp,   // stretch the edge texel
};

struct Sampler {
    SampleMode filter = SampleMode::Bilinear;
    WrapMode wrap = WrapMode::Repeat;
};

class Texture {
public:
    Texture() = default;
    Texture(int width, int height, std::vector<core::Color> pixels);

    // --- procedural constructors (usable without any image file) -------------
    static Texture solid(int width, int height, const core::Color& color);
    static Texture checker(int width, int height, int cells, const core::Color& a,
                           const core::Color& b);
    // uv grid: red ramps with u, green ramps with v, white lines every `cells`
    // texels. Meant for checking interpolation and wrapping by eye.
    static Texture uvGrid(int width, int height, int cells);

    // --- queries -------------------------------------------------------------
    int width() const { return width_; }
    int height() const { return height_; }
    bool empty() const { return width_ <= 0 || height_ <= 0 || pixels_.empty(); }
    size_t memoryBytes() const { return pixels_.size() * sizeof(core::Color); }

    // Texel access with wrapping applied (never reads out of bounds).
    const core::Color& texel(int x, int y, WrapMode wrap = WrapMode::Repeat) const;

    const std::vector<core::Color>& pixels() const { return pixels_; }

    // --- sampling ------------------------------------------------------------
    // uv is in texture space: (0,0) is the corner of texel (0,0), (1,1) the
    // opposite corner. Returns linear RGBA in [0, 1]. An empty texture samples
    // as opaque magenta so a missing texture is obvious instead of black.
    math::Vec4 sample(const math::Vec2& uv, const Sampler& sampler = {}) const;

private:
    int wrapIndex(int v, int size, WrapMode wrap) const;

    int width_ = 0;
    int height_ = 0;
    std::vector<core::Color> pixels_;
};

// Sampling helpers, also used by tests.
inline float wrapRepeat(float v) { return v - std::floor(v); }

}  // namespace nova::texture
