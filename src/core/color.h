// NOVA GPU - Software GPU
// src/core/color.h - 8-bit display-referred color.
#pragma once

#include <cstdint>

#include "math/vec.h"

namespace nova::core {

// 4 bytes per pixel, tightly packed. RGBA8, display-referred: NOVA does not
// perform a linear -> sRGB transfer because Phase 1 shades in display space.
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

inline uint8_t floatToByte(float v) {
    // Round-half-away-from-zero on the clamped [0, 1] range.
    const float c = math::clampf(v, 0.0f, 1.0f);
    return static_cast<uint8_t>(c * 255.0f + 0.5f);
}

inline Color colorFromFloat(float r, float g, float b, float a = 1.0f) {
    return Color{floatToByte(r), floatToByte(g), floatToByte(b), floatToByte(a)};
}

// Vector form used by shaders: components are normally in [0, 1], but values
// outside the range are clamped rather than wrapped.
inline Color colorFromVec4(const math::Vec4& v) {
    return colorFromFloat(v.x, v.y, v.z, v.w);
}

inline math::Vec4 colorToVec4(const Color& c) {
    constexpr float kInv = 1.0f / 255.0f;
    return {c.r * kInv, c.g * kInv, c.b * kInv, c.a * kInv};
}

inline Color lerpColor(const Color& a, const Color& b, float t) {
    return colorFromVec4(math::lerp(colorToVec4(a), colorToVec4(b), t));
}

inline bool operator==(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

inline bool operator!=(const Color& a, const Color& b) { return !(a == b); }

// Named constants used by demos, tests and the default clear color.
constexpr Color kBlack{0, 0, 0, 255};
constexpr Color kWhite{255, 255, 255, 255};
constexpr Color kRed{255, 0, 0, 255};
constexpr Color kGreen{0, 255, 0, 255};
constexpr Color kBlue{0, 0, 255, 255};
constexpr Color kYellow{255, 255, 0, 255};
constexpr Color kDarkGray{16, 16, 24, 255};

}  // namespace nova::core
