// NOVA GPU - Software GPU
// src/pipeline/frustum.h - view-frustum extraction and visibility tests.
//
// Visibility stage of the pipeline: before a draw call is processed, its world
// bounds are tested against the six frustum planes. Testing one box costs a
// handful of multiplies per plane, so rejecting a whole object pays for itself
// as soon as it has more than a few triangles - see docs/performance.md.
//
// Plane extraction is the standard Gribb-Hartmann method for a row-major,
// column-vector matrix M (clip = M * world), with NDC in [-1, 1]:
//
//   left   : x + w >= 0  ->  row0 + row3
//   right  : w - x >= 0  ->  row3 - row0
//   bottom : y + w >= 0  ->  row1 + row3
//   top    : w - y >= 0  ->  row3 - row1
//   near   : z + w >= 0  ->  row2 + row3
//   far    : w - z >= 0  ->  row3 - row2
//
// Each plane (a, b, c, d) satisfies a*x + b*y + c*z + d >= 0 for points inside.
#pragma once

#include "math/bounds.h"
#include "math/mat4.h"
#include "math/vec.h"

namespace nova::pipeline {

enum PlaneIndex { kLeft = 0, kRight, kBottom, kTop, kNear, kFar, kPlaneCount = 6 };

struct Frustum {
    math::Vec4 planes[kPlaneCount];  // inward-facing: inside is >= 0

    static Frustum fromViewProjection(const math::Mat4& m) {
        const math::Vec4 row0{m.m[0][0], m.m[0][1], m.m[0][2], m.m[0][3]};
        const math::Vec4 row1{m.m[1][0], m.m[1][1], m.m[1][2], m.m[1][3]};
        const math::Vec4 row2{m.m[2][0], m.m[2][1], m.m[2][2], m.m[2][3]};
        const math::Vec4 row3{m.m[3][0], m.m[3][1], m.m[3][2], m.m[3][3]};

        Frustum f;
        f.planes[kLeft] = row0 + row3;
        f.planes[kRight] = row3 - row0;
        f.planes[kBottom] = row1 + row3;
        f.planes[kTop] = row3 - row1;
        f.planes[kNear] = row2 + row3;
        f.planes[kFar] = row3 - row2;
        return f;
    }

    // True when the box is at least partially inside (or straddling) the frustum.
    // Conservative: it can return true for a box that turns out to be invisible,
    // but never false for a visible one.
    bool intersects(const math::Aabb& box) const {
        if (!box.valid()) return false;

        for (const math::Vec4& p : planes) {
            // The box corner furthest along the plane normal decides: if even
            // that corner is behind the plane, every point of the box is behind
            // it and the box is fully outside.
            const float x = p.x >= 0.0f ? box.max.x : box.min.x;
            const float y = p.y >= 0.0f ? box.max.y : box.min.y;
            const float z = p.z >= 0.0f ? box.max.z : box.min.z;
            if (p.x * x + p.y * y + p.z * z + p.w < 0.0f) return false;
        }
        return true;
    }

    bool intersectsSphere(const math::Vec3& center, float radius) const {
        for (const math::Vec4& p : planes) {
            const float d = p.x * center.x + p.y * center.y + p.z * center.z + p.w;
            if (d < -radius) return false;
        }
        return true;
    }

    bool intersectsSphere(const math::BoundingSphere& sphere) const {
        return intersectsSphere(sphere.center, sphere.radius);
    }
};

}  // namespace nova::pipeline
