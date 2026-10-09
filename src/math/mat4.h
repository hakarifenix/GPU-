// NOVA GPU - Software GPU
// src/math/mat4.h - 4x4 matrices.
//
// Convention (fixed and tested):
//   * row-major storage: m[row][col]
//   * column-vector maths: (M * v)
//   * right-handed world space, camera looks down -Z
//   * projection produces OpenGL-style NDC, z in [-1, 1]
#pragma once

#include <cmath>

#include "math/vec.h"

namespace nova::math {

struct Mat4 {
    // m[row][col]
    float m[4][4] = {};

    constexpr Mat4() = default;

    static Mat4 identity() {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    static Mat4 zero() { return Mat4{}; }

    float& at(int row, int col) { return m[row][col]; }
    float at(int row, int col) const { return m[row][col]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] +
                        a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
        }
    }
    return r;
}

// Transform a column vector: returns M * v.
inline Vec4 transform(const Mat4& a, const Vec4& v) {
    return {a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z + a.m[0][3] * v.w,
            a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z + a.m[1][3] * v.w,
            a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z + a.m[2][3] * v.w,
            a.m[3][0] * v.x + a.m[3][1] * v.y + a.m[3][2] * v.z + a.m[3][3] * v.w};
}

// Transform a point (implicit w = 1), returning a Vec3 (w is left untouched).
inline Vec3 transformPoint(const Mat4& a, const Vec3& p) {
    return {a.m[0][0] * p.x + a.m[0][1] * p.y + a.m[0][2] * p.z + a.m[0][3],
            a.m[1][0] * p.x + a.m[1][1] * p.y + a.m[1][2] * p.z + a.m[1][3],
            a.m[2][0] * p.x + a.m[2][1] * p.y + a.m[2][2] * p.z + a.m[2][3]};
}

// Transform a direction (implicit w = 0).
inline Vec3 transformDirection(const Mat4& a, const Vec3& v) {
    return {a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z,
            a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z,
            a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z};
}

inline Mat4 transpose(const Mat4& a) {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) r.m[i][j] = a.m[j][i];
    return r;
}

inline Mat4 translation(const Vec3& t) {
    Mat4 r = Mat4::identity();
    r.m[0][3] = t.x;
    r.m[1][3] = t.y;
    r.m[2][3] = t.z;
    return r;
}

inline Mat4 scaling(const Vec3& s) {
    Mat4 r = Mat4::identity();
    r.m[0][0] = s.x;
    r.m[1][1] = s.y;
    r.m[2][2] = s.z;
    return r;
}

inline Mat4 rotationX(float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    Mat4 r = Mat4::identity();
    r.m[1][1] = c;  r.m[1][2] = -s;
    r.m[2][1] = s;  r.m[2][2] = c;
    return r;
}

inline Mat4 rotationY(float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    Mat4 r = Mat4::identity();
    r.m[0][0] = c;  r.m[0][2] = s;
    r.m[2][0] = -s; r.m[2][2] = c;
    return r;
}

inline Mat4 rotationZ(float radians) {
    const float c = std::cos(radians), s = std::sin(radians);
    Mat4 r = Mat4::identity();
    r.m[0][0] = c;  r.m[0][1] = -s;
    r.m[1][0] = s;  r.m[1][1] = c;
    return r;
}

// Right-handed perspective projection (OpenGL convention).
// Maps the view frustum to NDC with x,y,z in [-1, 1]; visible when w > 0.
inline Mat4 perspective(float fovYRadians, float aspect, float nearZ, float farZ) {
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 r;
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = (farZ + nearZ) / (nearZ - farZ);
    r.m[2][3] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    r.m[3][2] = -1.0f;
    r.m[3][3] = 0.0f;
    return r;
}

// Right-handed view matrix.
inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 r = Mat4::identity();
    r.m[0][0] = s.x; r.m[0][1] = s.y; r.m[0][2] = s.z; r.m[0][3] = -dot(s, eye);
    r.m[1][0] = u.x; r.m[1][1] = u.y; r.m[1][2] = u.z; r.m[1][3] = -dot(u, eye);
    r.m[2][0] = -f.x; r.m[2][1] = -f.y; r.m[2][2] = -f.z; r.m[2][3] = dot(f, eye);
    return r;
}

}  // namespace nova::math
