// NOVA GPU - Software GPU
// src/camera/camera.h - perspective camera producing view and projection matrices.
//
// Convention follows math/mat4.h: right-handed world space, the camera looks
// down -Z, and projection yields OpenGL-style NDC with z in [-1, 1].
#pragma once

#include "math/mat4.h"

namespace nova::camera {

class Camera {
public:
    Camera() = default;

    // --- setup -------------------------------------------------------------
    void setPerspective(float fovYDegrees, float aspect, float nearZ, float farZ);
    void setAspect(float aspect);
    void setNearFar(float nearZ, float farZ);

    void setPosition(const math::Vec3& eye) { position_ = eye; }
    void setTarget(const math::Vec3& target) { target_ = target; }
    void setUp(const math::Vec3& up) { up_ = up; }

    // --- queries -----------------------------------------------------------
    const math::Vec3& position() const { return position_; }
    const math::Vec3& target() const { return target_; }
    float fovYRadians() const { return fovY_; }
    float aspect() const { return aspect_; }
    float nearZ() const { return near_; }
    float farZ() const { return far_; }

    math::Mat4 view() const { return math::lookAt(position_, target_, up_); }
    math::Mat4 projection() const { return math::perspective(fovY_, aspect_, near_, far_); }
    math::Mat4 viewProjection() const { return projection() * view(); }

    // Unit vector from the eye towards the target.
    math::Vec3 forward() const { return math::normalize(target_ - position_); }

private:
    math::Vec3 position_{0.0f, 0.0f, 3.0f};
    math::Vec3 target_{0.0f, 0.0f, 0.0f};
    math::Vec3 up_{0.0f, 1.0f, 0.0f};
    float fovY_ = 1.0471975512f;  // 60 degrees
    float aspect_ = 4.0f / 3.0f;
    float near_ = 0.1f;
    float far_ = 100.0f;
};

}  // namespace nova::camera
