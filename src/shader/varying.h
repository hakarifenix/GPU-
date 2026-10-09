// NOVA GPU - Software GPU
// src/shader/varying.h - the inter-stage payload produced by a vertex shader and
// consumed by a fragment shader ("varyings" in GPU terms).
//
// This is NOVA's small internal shader representation. It is a plain struct of
// named attributes rather than a GLSL compiler: easy to read, trivial to
// interpolate, and extended by adding a field plus one line in the vertex
// shader. Clipping interpolates it in clip space; the rasterizer interpolates it
// perspective-correctly in screen space.
#pragma once

#include "math/vec.h"

namespace nova::shader {

struct Varying {
    math::Vec3 worldPosition{0.0f, 0.0f, 0.0f};  // world-space position
    math::Vec3 normal{0.0f, 0.0f, 1.0f};         // world-space normal (not normalized)
    math::Vec3 color{1.0f, 1.0f, 1.0f};          // per-vertex color
    math::Vec2 uv{0.0f, 0.0f};                   // texture coordinate
};

// Component-wise helpers. Keeping them here means the clipper, triangle setup
// and rasterizer all interpolate a new attribute the same way, automatically.
inline Varying operator+(const Varying& a, const Varying& b) {
    Varying out;
    out.worldPosition = a.worldPosition + b.worldPosition;
    out.normal = a.normal + b.normal;
    out.color = a.color + b.color;
    out.uv = a.uv + b.uv;
    return out;
}

inline Varying operator*(const Varying& v, float s) {
    Varying out;
    out.worldPosition = v.worldPosition * s;
    out.normal = v.normal * s;
    out.color = v.color * s;
    out.uv = v.uv * s;
    return out;
}

inline Varying lerpVarying(const Varying& a, const Varying& b, float t) {
    Varying out;
    out.worldPosition = math::lerp(a.worldPosition, b.worldPosition, t);
    out.normal = math::lerp(a.normal, b.normal, t);
    out.color = math::lerp(a.color, b.color, t);
    out.uv = math::lerp(a.uv, b.uv, t);
    return out;
}

}  // namespace nova::shader
