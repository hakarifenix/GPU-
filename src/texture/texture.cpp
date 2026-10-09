#include "texture/texture.h"

#include <cmath>
#include <utility>

namespace nova::texture {
namespace {

const math::Vec4 kMissingTexture{1.0f, 0.0f, 1.0f, 1.0f};  // opaque magenta

}  // namespace

Texture::Texture(int width, int height, std::vector<core::Color> pixels)
    : width_(width), height_(height), pixels_(std::move(pixels)) {
    if (width_ < 0) width_ = 0;
    if (height_ < 0) height_ = 0;
    const size_t expected = static_cast<size_t>(width_) * static_cast<size_t>(height_);
    if (pixels_.size() != expected) {
        // A mismatched pixel count would silently shift every texel; drop the
        // contents instead so the texture reads as empty (magenta).
        width_ = 0;
        height_ = 0;
        pixels_.clear();
    }
}

Texture Texture::solid(int width, int height, const core::Color& color) {
    if (width <= 0 || height <= 0) return Texture();
    return Texture(width, height,
                   std::vector<core::Color>(static_cast<size_t>(width) * static_cast<size_t>(height),
                                            color));
}

Texture Texture::checker(int width, int height, int cells, const core::Color& a,
                         const core::Color& b) {
    if (width <= 0 || height <= 0) return Texture();
    if (cells <= 0) cells = 1;

    std::vector<core::Color> pixels(static_cast<size_t>(width) * static_cast<size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int cx = (x * cells) / width;
            const int cy = (y * cells) / height;
            const bool odd = ((cx + cy) & 1) != 0;
            pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] =
                odd ? b : a;
        }
    }
    return Texture(width, height, std::move(pixels));
}

Texture Texture::uvGrid(int width, int height, int cells) {
    if (width <= 0 || height <= 0) return Texture();
    if (cells <= 0) cells = 1;

    std::vector<core::Color> pixels(static_cast<size_t>(width) * static_cast<size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float u = (x + 0.5f) / static_cast<float>(width);
            const float v = (y + 0.5f) / static_cast<float>(height);
            // A white grid line on the texel where each cell starts.
            const int cellWidth = width / cells > 0 ? width / cells : 1;
            const int cellHeight = height / cells > 0 ? height / cells : 1;
            const bool line = (x % cellWidth == 0) || (y % cellHeight == 0);
            const core::Color c =
                line ? core::kWhite : core::colorFromFloat(u, v, 0.25f + 0.5f * (1.0f - u));
            pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = c;
        }
    }
    return Texture(width, height, std::move(pixels));
}

int Texture::wrapIndex(int v, int size, WrapMode wrap) const {
    if (size <= 0) return 0;
    if (wrap == WrapMode::Repeat) {
        v %= size;
        if (v < 0) v += size;
        return v;
    }
    if (v < 0) return 0;
    if (v >= size) return size - 1;
    return v;
}

const core::Color& Texture::texel(int x, int y, WrapMode wrap) const {
    static const core::Color kMissing{255, 0, 255, 255};
    if (empty()) return kMissing;
    const int sx = wrapIndex(x, width_, wrap);
    const int sy = wrapIndex(y, height_, wrap);
    return pixels_[static_cast<size_t>(sy) * static_cast<size_t>(width_) + static_cast<size_t>(sx)];
}

math::Vec4 Texture::sample(const math::Vec2& uv, const Sampler& sampler) const {
    if (empty()) return kMissingTexture;

    // Resolve wrapping on the uv coordinate first: repeat keeps the fractional
    // part (so u = 1.0 lands back on texel 0), clamp pins to the [0, 1] edge.
    const float u = sampler.wrap == WrapMode::Repeat ? wrapRepeat(uv.x) : math::clampf(uv.x, 0.0f, 1.0f);
    const float v = sampler.wrap == WrapMode::Repeat ? wrapRepeat(uv.y) : math::clampf(uv.y, 0.0f, 1.0f);

    const float fx = u * static_cast<float>(width_);
    const float fy = v * static_cast<float>(height_);

    if (sampler.filter == SampleMode::Nearest) {
        const int x = static_cast<int>(std::floor(fx));
        const int y = static_cast<int>(std::floor(fy));
        // The coordinate is already resolved, so this only has to clamp the
        // u == 1 (clamp mode) case back onto the last texel.
        return core::colorToVec4(texel(x, y, sampler.wrap));
    }

    // Bilinear: sample at texel centers (half-texel offset), then blend.
    const float sx = fx - 0.5f;
    const float sy = fy - 0.5f;
    const int x0 = static_cast<int>(std::floor(sx));
    const int y0 = static_cast<int>(std::floor(sy));
    const float tx = sx - static_cast<float>(x0);
    const float ty = sy - static_cast<float>(y0);

    const math::Vec4 c00 = core::colorToVec4(texel(x0, y0, sampler.wrap));
    const math::Vec4 c10 = core::colorToVec4(texel(x0 + 1, y0, sampler.wrap));
    const math::Vec4 c01 = core::colorToVec4(texel(x0, y0 + 1, sampler.wrap));
    const math::Vec4 c11 = core::colorToVec4(texel(x0 + 1, y0 + 1, sampler.wrap));

    const math::Vec4 top = math::lerp(c00, c10, tx);
    const math::Vec4 bottom = math::lerp(c01, c11, tx);
    return math::lerp(top, bottom, ty);
}

}  // namespace nova::texture
