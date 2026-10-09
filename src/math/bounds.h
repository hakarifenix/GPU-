// NOVA GPU - Software GPU
// src/math/bounds.h - axis-aligned bounds and bounding spheres.
//
// Used by the visibility stage: an object whose bounds are outside the view
// frustum can be rejected before any vertex is touched. The bounds are plain
// data with no knowledge of meshes, so the math stays testable in isolation.
#pragma once

#include <algorithm>
#include <limits>

#include "math/mat4.h"
#include "math/vec.h"

namespace nova::math {

struct Aabb {
    Vec3 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest()};

    bool valid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }

    void expand(const Vec3& p) {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }

    void expand(const Aabb& other) {
        if (!other.valid()) return;
        expand(other.min);
        expand(other.max);
    }

    Vec3 center() const { return (min + max) * 0.5f; }
    Vec3 extent() const { return max - min; }

    // Radius of the sphere that contains the box, centred on the box centre.
    float radius() const { return length(extent()) * 0.5f; }

    // Transforms the eight corners and re-fits an axis-aligned box around them.
    // Conservative for rotation (the result is larger than the rotated box),
    // which is exactly what a visibility test wants.
    Aabb transformed(const Mat4& m) const {
        if (!valid()) return {};
        Aabb out;
        for (int c = 0; c < 8; ++c) {
            const Vec3 corner{(c & 1) ? max.x : min.x, (c & 2) ? max.y : min.y,
                              (c & 4) ? max.z : min.z};
            out.expand(transformPoint(m, corner));
        }
        return out;
    }
};

struct BoundingSphere {
    Vec3 center{0.0f, 0.0f, 0.0f};
    float radius = 0.0f;

    bool valid() const { return radius >= 0.0f; }
};

// Scales a bounding sphere by a transform: the centre follows the transform and
// the radius grows by the largest axis scale, so the result always contains the
// transformed geometry.
inline BoundingSphere transformSphere(const BoundingSphere& sphere, const Mat4& m) {
    BoundingSphere out;
    out.center = transformPoint(m, sphere.center);
    const float sx = length(Vec3{m.m[0][0], m.m[1][0], m.m[2][0]});
    const float sy = length(Vec3{m.m[0][1], m.m[1][1], m.m[2][1]});
    const float sz = length(Vec3{m.m[0][2], m.m[1][2], m.m[2][2]});
    out.radius = sphere.radius * std::max(sx, std::max(sy, sz));
    return out;
}

}  // namespace nova::math
