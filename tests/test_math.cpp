// NOVA GPU - Software GPU
// tests/test_math.cpp - vector and matrix maths.
#include "test_framework.h"

#include "math/mat4.h"
#include "math/math_utils.h"
#include "math/vec.h"

using namespace nova::math;

NOVA_TEST(Vec, Arithmetic) {
    const Vec3 a{1.0f, 2.0f, 3.0f};
    const Vec3 b{4.0f, 5.0f, 6.0f};

    CHECK_EQ((a + b).x, 5.0f);
    CHECK_EQ((a + b).y, 7.0f);
    CHECK_EQ((a + b).z, 9.0f);
    CHECK_EQ((b - a).x, 3.0f);
    CHECK_EQ((a * 2.0f).z, 6.0f);
    CHECK_EQ((a / 2.0f).y, 1.0f);
    CHECK_EQ((-a).x, -1.0f);
    CHECK_EQ((a * b).y, 10.0f);  // component-wise
}

NOVA_TEST(Vec, DotAndCross) {
    const Vec3 x{1.0f, 0.0f, 0.0f};
    const Vec3 y{0.0f, 1.0f, 0.0f};

    CHECK_EQ(dot(x, y), 0.0f);
    CHECK_EQ(dot(x, x), 1.0f);
    CHECK_EQ(dot(Vec3{1, 2, 3}, Vec3{4, 5, 6}), 32.0f);

    const Vec3 z = cross(x, y);
    CHECK_EQ(z.x, 0.0f);
    CHECK_EQ(z.y, 0.0f);
    CHECK_EQ(z.z, 1.0f);
    // Right-handed: X x Y == Z, Y x Z == X.
    CHECK(cross(y, z).x == 1.0f);
}

NOVA_TEST(Vec, LengthAndNormalize) {
    CHECK_NEAR(length(Vec3{3.0f, 4.0f, 0.0f}), 5.0f, 1e-5);
    CHECK_NEAR(lengthSquared(Vec3{1.0f, 2.0f, 2.0f}), 9.0f, 1e-6);

    const Vec3 n = normalize(Vec3{0.0f, 0.0f, 5.0f});
    CHECK_NEAR(n.z, 1.0f, 1e-6);
    CHECK_NEAR(length(n), 1.0f, 1e-6);

    // Normalizing zero must not produce NaN.
    const Vec3 zero = normalize(Vec3{0.0f, 0.0f, 0.0f});
    CHECK(zero.x == 0.0f && zero.y == 0.0f && zero.z == 0.0f);
}

NOVA_TEST(Vec, LerpAndClamp) {
    const Vec3 a{0.0f, 0.0f, 0.0f};
    const Vec3 b{10.0f, 20.0f, 30.0f};
    CHECK_NEAR(lerp(a, b, 0.5f).y, 10.0f, 1e-6);
    CHECK_NEAR(lerp(a, b, 1.0f).z, 30.0f, 1e-6);
    CHECK_EQ(clampf(5.0f, 0.0f, 1.0f), 1.0f);
    CHECK_EQ(clampf(-5.0f, 0.0f, 1.0f), 0.0f);
    CHECK_EQ(clampf(0.5f, 0.0f, 1.0f), 0.5f);
    CHECK_NEAR(radians(180.0f), kPi, 1e-5);
}

NOVA_TEST(Mat4, IdentityAndMultiply) {
    const Mat4 i = Mat4::identity();
    const Mat4 m = translation({1.0f, 2.0f, 3.0f});

    const Mat4 product = i * m;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            CHECK_NEAR(product.at(r, c), m.at(r, c), 1e-6);
        }
    }

    // (m * m) is a translation of twice the vector.
    const Mat4 squared = m * m;
    CHECK_NEAR(squared.at(0, 3), 2.0f, 1e-6);
    CHECK_NEAR(squared.at(1, 3), 4.0f, 1e-6);
    CHECK_NEAR(squared.at(2, 3), 6.0f, 1e-6);
}

NOVA_TEST(Mat4, TransformPointAndDirection) {
    const Mat4 t = translation({5.0f, 0.0f, 0.0f});
    const Vec3 point = transformPoint(t, Vec3{1.0f, 1.0f, 1.0f});
    CHECK_NEAR(point.x, 6.0f, 1e-6);
    CHECK_NEAR(point.y, 1.0f, 1e-6);

    // Directions ignore translation.
    const Vec3 direction = transformDirection(t, Vec3{1.0f, 0.0f, 0.0f});
    CHECK_NEAR(direction.x, 1.0f, 1e-6);

    const Vec4 homogeneous = transform(t, Vec4{1.0f, 1.0f, 1.0f, 1.0f});
    CHECK_NEAR(homogeneous.w, 1.0f, 1e-6);
    CHECK_NEAR(homogeneous.x, 6.0f, 1e-6);
}

NOVA_TEST(Mat4, RotationY) {
    const Mat4 r = rotationY(kPi * 0.5f);
    const Vec3 rotated = transformDirection(r, Vec3{1.0f, 0.0f, 0.0f});
    CHECK_NEAR(rotated.x, 0.0f, 1e-6);
    CHECK_NEAR(rotated.z, -1.0f, 1e-6);
    CHECK_NEAR(length(rotated), 1.0f, 1e-6);
}

NOVA_TEST(Mat4, PerspectiveMapsNearAndFarToNdc) {
    const float nearZ = 0.1f;
    const float farZ = 100.0f;
    const Mat4 p = perspective(radians(60.0f), 4.0f / 3.0f, nearZ, farZ);

    // A point on the near plane in view space -> NDC z = -1.
    const Vec4 nearClip = transform(p, Vec4{0.0f, 0.0f, -nearZ, 1.0f});
    CHECK_NEAR(nearClip.w, nearZ, 1e-5);
    CHECK_NEAR(nearClip.z / nearClip.w, -1.0f, 1e-4);

    // A point on the far plane -> NDC z = +1.
    const Vec4 farClip = transform(p, Vec4{0.0f, 0.0f, -farZ, 1.0f});
    CHECK_NEAR(farClip.w, farZ, 1e-3);
    CHECK_NEAR(farClip.z / farClip.w, 1.0f, 1e-3);

    // The frustum edges map to the NDC box at the near plane.
    const float halfHeight = nearZ * std::tan(radians(60.0f) * 0.5f);
    const float halfWidth = halfHeight * (4.0f / 3.0f);
    const Vec4 edge = transform(p, Vec4{halfWidth, halfHeight, -nearZ, 1.0f});
    CHECK_NEAR(edge.x / edge.w, 1.0f, 1e-4);
    CHECK_NEAR(edge.y / edge.w, 1.0f, 1e-4);
}

NOVA_TEST(Mat4, LookAtPlacesTargetOnNegativeZ) {
    const Mat4 view = lookAt({0.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    const Vec3 origin = transformPoint(view, {0.0f, 0.0f, 0.0f});
    CHECK_NEAR(origin.x, 0.0f, 1e-5);
    CHECK_NEAR(origin.y, 0.0f, 1e-5);
    CHECK_NEAR(origin.z, -3.0f, 1e-5);

    // A point to the right of the camera stays on +X.
    const Vec3 right = transformPoint(view, {1.0f, 0.0f, 0.0f});
    CHECK_NEAR(right.x, 1.0f, 1e-5);
}

NOVA_TEST(Mat4, TransposeSwapsElements) {
    Mat4 m = Mat4::zero();
    m.at(0, 1) = 3.0f;
    m.at(2, 3) = 7.0f;
    const Mat4 t = transpose(m);
    CHECK_NEAR(t.at(1, 0), 3.0f, 1e-6);
    CHECK_NEAR(t.at(3, 2), 7.0f, 1e-6);
    CHECK_NEAR(t.at(0, 1), 0.0f, 1e-6);
}
