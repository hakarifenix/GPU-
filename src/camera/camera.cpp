#include "camera/camera.h"

#include "math/math_utils.h"

#include <stdexcept>
#include <string>

namespace nova::camera {

void Camera::setPerspective(float fovYDegrees, float aspect, float nearZ, float farZ) {
    if (aspect <= 0.0f) throw std::invalid_argument("Camera: aspect must be > 0");
    if (nearZ <= 0.0f) throw std::invalid_argument("Camera: near plane must be > 0");
    if (farZ <= nearZ) throw std::invalid_argument("Camera: far plane must be > near plane");
    if (fovYDegrees <= 0.0f || fovYDegrees >= 180.0f)
        throw std::invalid_argument("Camera: vertical field of view must be in (0, 180) degrees");

    fovY_ = math::radians(fovYDegrees);
    aspect_ = aspect;
    near_ = nearZ;
    far_ = farZ;
}

void Camera::setAspect(float aspect) {
    if (aspect <= 0.0f) throw std::invalid_argument("Camera: aspect must be > 0");
    aspect_ = aspect;
}

void Camera::setNearFar(float nearZ, float farZ) {
    if (nearZ <= 0.0f || farZ <= nearZ)
        throw std::invalid_argument("Camera: require 0 < near < far");
    near_ = nearZ;
    far_ = farZ;
}

}  // namespace nova::camera
