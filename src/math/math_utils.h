// NOVA GPU - Software GPU
// src/math/math_utils.h - angle helpers and barycentric coordinates.
#pragma once

#include "math/vec.h"

namespace nova::math {

constexpr float kPi = 3.14159265358979323846f;

constexpr float radians(float degrees) { return degrees * (kPi / 180.0f); }
constexpr float degrees(float radians_) { return radians_ * (180.0f / kPi); }

// Edge function of the point p against the directed edge a -> b:
//   E(a, b, p) = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)
// It equals twice the signed area of triangle (a, b, p): positive when p is
// on the left of a->b in a y-up system, negative in a y-down (screen) system.
// "Twice the signed triangle area" is simply E(a, b, c).
inline float edgeFunction(const Vec2& a, const Vec2& b, const Vec2& p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

struct Barycentric {
    float u = 0.0f;  // weight of vertex 0
    float v = 0.0f;  // weight of vertex 1
    float w = 0.0f;  // weight of vertex 2

    // Weights sum to 1 for points inside the triangle's plane.
    float sum() const { return u + v + w; }
};

// Barycentric coordinates of p with respect to triangle (a, b, c).
// Returns u = v = w = 0 when the triangle is degenerate (zero area).
inline Barycentric computeBarycentric(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& p) {
    const float area = edgeFunction(a, b, c);
    if (area == 0.0f) return Barycentric{};

    const float invArea = 1.0f / area;
    // Sub-triangle (p, b, c) area -> weight of a, etc.
    Barycentric out;
    out.u = edgeFunction(b, c, p) * invArea;
    out.v = edgeFunction(c, a, p) * invArea;
    out.w = edgeFunction(a, b, p) * invArea;
    return out;
}

inline bool isInsideTriangle(const Barycentric& bary) {
    return bary.u >= 0.0f && bary.v >= 0.0f && bary.w >= 0.0f;
}

}  // namespace nova::math
