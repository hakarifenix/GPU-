// NOVA GPU - Software GPU
// tests/test_barycentric.cpp - edge functions and barycentric coordinates.
#include "test_framework.h"

#include "math/math_utils.h"

using namespace nova::math;

namespace {
// Counter-clockwise triangle in a y-up sense.
constexpr Vec2 kA{0.0f, 0.0f};
constexpr Vec2 kB{1.0f, 0.0f};
constexpr Vec2 kC{0.0f, 1.0f};
}  // namespace

NOVA_TEST(Barycentric, EdgeFunctionSign) {
    // A, B, C is counter-clockwise -> positive area.
    CHECK(edgeFunction(kA, kB, kC) > 0.0f);
    // Swapping two vertices flips the sign.
    CHECK(edgeFunction(kB, kA, kC) < 0.0f);
    // Collinear points have zero area.
    CHECK_NEAR(edgeFunction({0, 0}, {1, 1}, {2, 2}), 0.0f, 1e-6);
    // Twice the signed area of a unit right triangle is 1.
    CHECK_NEAR(edgeFunction(kA, kB, kC), 1.0f, 1e-6);
}

NOVA_TEST(Barycentric, AtVertices) {
    const Barycentric atA = computeBarycentric(kA, kB, kC, kA);
    CHECK_NEAR(atA.u, 1.0f, 1e-5);
    CHECK_NEAR(atA.v, 0.0f, 1e-5);
    CHECK_NEAR(atA.w, 0.0f, 1e-5);

    const Barycentric atB = computeBarycentric(kA, kB, kC, kB);
    CHECK_NEAR(atB.v, 1.0f, 1e-5);

    const Barycentric atC = computeBarycentric(kA, kB, kC, kC);
    CHECK_NEAR(atC.w, 1.0f, 1e-5);
}

NOVA_TEST(Barycentric, CentroidIsEqualWeighted) {
    const Vec2 centroid{(kA.x + kB.x + kC.x) / 3.0f, (kA.y + kB.y + kC.y) / 3.0f};
    const Barycentric b = computeBarycentric(kA, kB, kC, centroid);
    CHECK_NEAR(b.u, 1.0f / 3.0f, 1e-5);
    CHECK_NEAR(b.v, 1.0f / 3.0f, 1e-5);
    CHECK_NEAR(b.w, 1.0f / 3.0f, 1e-5);
    CHECK_NEAR(b.sum(), 1.0f, 1e-5);
}

NOVA_TEST(Barycentric, InsideOutsideAndWeightsSumToOne) {
    const Vec2 inside{0.25f, 0.25f};
    CHECK(isInsideTriangle(computeBarycentric(kA, kB, kC, inside)));
    CHECK_NEAR(computeBarycentric(kA, kB, kC, inside).sum(), 1.0f, 1e-5);

    const Vec2 outside{1.0f, 1.0f};
    CHECK(!isInsideTriangle(computeBarycentric(kA, kB, kC, outside)));
}

NOVA_TEST(Barycentric, InterpolationReproducesLinearField) {
    // A linear function over a triangle must be reproduced exactly by the
    // barycentric weights, which is what the rasterizer relies on.
    const float fa = 2.0f, fb = 5.0f, fc = -3.0f;
    const Vec2 p{0.2f, 0.3f};
    const Barycentric b = computeBarycentric(kA, kB, kC, p);

    const float interpolated = b.u * fa + b.v * fb + b.w * fc;
    // Expected: f(x, y) = fa + (fb - fa) * x + (fc - fa) * y
    const float expected = fa + (fb - fa) * p.x + (fc - fa) * p.y;
    CHECK_NEAR(interpolated, expected, 1e-5);
}

NOVA_TEST(Barycentric, DegenerateTriangleIsSafe) {
    const Barycentric b = computeBarycentric({0, 0}, {1, 1}, {2, 2}, {1, 0});
    CHECK_NEAR(b.u, 0.0f, 1e-6);
    CHECK_NEAR(b.v, 0.0f, 1e-6);
    CHECK_NEAR(b.w, 0.0f, 1e-6);
}
