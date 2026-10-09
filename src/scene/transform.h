// NOVA GPU - Software GPU
// src/scene/transform.h - model matrix helper.
#pragma once

#include "math/mat4.h"

namespace nova::scene {

// Model matrix from translation, Euler rotation (radians, applied X then Y then
// Z) and non-uniform scale: M = T * Rz * Ry * Rx * S.
inline math::Mat4 makeTransform(const math::Vec3& translation = {},
                                const math::Vec3& rotationRadians = {},
                                const math::Vec3& scale = {1.0f, 1.0f, 1.0f}) {
    math::Mat4 m = math::translation(translation);
    if (rotationRadians.z != 0.0f) m = m * math::rotationZ(rotationRadians.z);
    if (rotationRadians.y != 0.0f) m = m * math::rotationY(rotationRadians.y);
    if (rotationRadians.x != 0.0f) m = m * math::rotationX(rotationRadians.x);
    return m * math::scaling(scale);
}

}  // namespace nova::scene
