// NOVA GPU - Software GPU
// tests/test_pipeline.cpp - viewport transform and triangle setup.
#include "test_framework.h"

#include "pipeline/triangle_setup.h"
#include "pipeline/viewport.h"

using namespace nova::math;
using namespace nova::pipeline;

namespace {

// Builds a clip-space vertex with w = 1 (so NDC == xyz).
ClipVertex clipVertex(float x, float y, float z, const Vec3& color = {1.0f, 1.0f, 1.0f}) {
    ClipVertex v;
    v.clip = {x, y, z, 1.0f};
    v.varying.worldPosition = {x, y, z};
    v.varying.normal = {0.0f, 0.0f, 1.0f};
    v.varying.color = color;
    v.varying.uv = {0.0f, 0.0f};
    return v;
}

Viewport viewport64() {
    Viewport vp;
    vp.width = 64.0f;
    vp.height = 64.0f;
    return vp;
}

}  // namespace

NOVA_TEST(Viewport, NdcToScreenFlipsY) {
    const Viewport vp = viewport64();
    const Vec2 bottomLeft = vp.ndcToScreen({-1.0f, -1.0f});
    CHECK_NEAR(bottomLeft.x, 0.0f, 1e-4);
    CHECK_NEAR(bottomLeft.y, 64.0f, 1e-4);  // NDC -1 is the bottom of the image

    const Vec2 topRight = vp.ndcToScreen({1.0f, 1.0f});
    CHECK_NEAR(topRight.x, 64.0f, 1e-4);
    CHECK_NEAR(topRight.y, 0.0f, 1e-4);

    const Vec2 center = vp.ndcToScreen({0.0f, 0.0f});
    CHECK_NEAR(center.x, 32.0f, 1e-4);
    CHECK_NEAR(center.y, 32.0f, 1e-4);
}

NOVA_TEST(Viewport, DepthMapping) {
    const Viewport vp = viewport64();
    CHECK_NEAR(vp.ndcToDepth(-1.0f), 0.0f, 1e-5);
    CHECK_NEAR(vp.ndcToDepth(1.0f), 1.0f, 1e-5);
    CHECK_NEAR(vp.ndcToDepth(0.0f), 0.5f, 1e-5);
    // Values outside the frustum clamp rather than wrap.
    CHECK_NEAR(vp.ndcToDepth(-5.0f), 0.0f, 1e-5);
    CHECK_NEAR(vp.ndcToDepth(5.0f), 1.0f, 1e-5);
}

NOVA_TEST(TriangleSetup, CounterClockwiseInNdcIsFrontFacing) {
    ScreenTriangle screen;
    // CCW in a y-up NDC: (-0.5,-0.5) -> (0.5,-0.5) -> (0.0,0.5)
    const ClipVertex in[3] = {clipVertex(-0.5f, -0.5f, 0.0f), clipVertex(0.5f, -0.5f, 0.0f),
                              clipVertex(0.0f, 0.5f, 0.0f)};
    const SetupResult result = setupTriangle(in, viewport64(), true, true, screen);
    CHECK(result == SetupResult::Ok);

    // Screen space positions (y flipped).
    CHECK_NEAR(screen.pos[0].x, 16.0f, 1e-3);
    CHECK_NEAR(screen.pos[0].y, 48.0f, 1e-3);
    CHECK_NEAR(screen.pos[1].x, 48.0f, 1e-3);
    CHECK_NEAR(screen.pos[1].y, 48.0f, 1e-3);
    CHECK_NEAR(screen.pos[2].x, 32.0f, 1e-3);
    CHECK_NEAR(screen.pos[2].y, 16.0f, 1e-3);
    CHECK(screen.area > 0.0f);

    // Bounding box of the coverable pixel centers: x 16..47, y 16..47.
    CHECK_EQ(screen.minX, 16);
    CHECK_EQ(screen.maxX, 47);
    CHECK_EQ(screen.minY, 16);
    CHECK_EQ(screen.maxY, 47);
}

NOVA_TEST(TriangleSetup, ClockwiseIsBackFacingAndCulled) {
    ScreenTriangle screen;
    // Same triangle with two vertices swapped -> clockwise in NDC (+y up).
    const ClipVertex in[3] = {clipVertex(0.5f, -0.5f, 0.0f), clipVertex(-0.5f, -0.5f, 0.0f),
                              clipVertex(0.0f, 0.5f, 0.0f)};
    CHECK(setupTriangle(in, viewport64(), true, true, screen) == SetupResult::Culled);

    // With culling disabled the same triangle is accepted.
    ScreenTriangle screenNoCull;
    CHECK(setupTriangle(in, viewport64(), true, false, screenNoCull) == SetupResult::Ok);
}

NOVA_TEST(TriangleSetup, DegenerateTriangleRejected) {
    ScreenTriangle screen;
    const ClipVertex in[3] = {clipVertex(0.0f, 0.0f, 0.0f), clipVertex(0.5f, 0.5f, 0.0f),
                              clipVertex(1.0f, 1.0f, 0.0f)};
    CHECK(setupTriangle(in, viewport64(), true, false, screen) == SetupResult::Degenerate);
}

NOVA_TEST(TriangleSetup, BarycentricWeightsAreConsistent) {
    ScreenTriangle screen;
    const ClipVertex in[3] = {clipVertex(-0.5f, -0.5f, 0.0f), clipVertex(0.5f, -0.5f, 0.0f),
                              clipVertex(0.0f, 0.5f, 0.0f)};
    CHECK(setupTriangle(in, viewport64(), true, true, screen) == SetupResult::Ok);

    // At a vertex, its own weight is 1 and the others are 0. Evaluating the
    // weight planes reproduces the barycentric coordinates the rasterizer uses.
    for (int v = 0; v < 3; ++v) {
        for (int i = 0; i < 3; ++i) {
            const float w = screen.weightA[i] * screen.pos[v].x + screen.weightB[i] * screen.pos[v].y +
                            screen.weightC[i];
            CHECK_NEAR(w, (i == v) ? 1.0f : 0.0f, 1e-4);
        }
    }

    // The three weights sum to 1 everywhere (affine partition of unity).
    const Vec2 probe{21.3f, 33.7f};
    float sum = 0.0f;
    for (int i = 0; i < 3; ++i) {
        sum += screen.weightA[i] * probe.x + screen.weightB[i] * probe.y + screen.weightC[i];
    }
    CHECK_NEAR(sum, 1.0f, 1e-4);

    // The centroid is inside; a corner of the bounding box is outside.
    const Vec2 centroid{(screen.pos[0].x + screen.pos[1].x + screen.pos[2].x) / 3.0f,
                        (screen.pos[0].y + screen.pos[1].y + screen.pos[2].y) / 3.0f};
    float inside = 1.0f;
    for (int i = 0; i < 3; ++i) {
        inside = std::min(inside,
                          screen.weightA[i] * centroid.x + screen.weightB[i] * centroid.y +
                              screen.weightC[i]);
    }
    CHECK(inside > 0.0f);

    // A point outside the triangle has at least one negative weight.
    float minWeight = 1.0f;
    const Vec2 corner{static_cast<float>(screen.minX) + 0.5f, static_cast<float>(screen.minY) + 0.5f};
    for (int i = 0; i < 3; ++i) {
        minWeight = std::min(minWeight,
                             screen.weightA[i] * corner.x + screen.weightB[i] * corner.y +
                                 screen.weightC[i]);
    }
    CHECK(minWeight < 0.0f);
}

NOVA_TEST(TriangleSetup, DepthAndInverseWAreStored) {
    ScreenTriangle screen;
    // w = 2 halves invW; depth comes from z / w.
    ClipVertex in[3] = {clipVertex(-0.5f, -0.5f, 0.0f), clipVertex(0.5f, -0.5f, 0.0f),
                        clipVertex(0.0f, 0.5f, 0.0f)};
    in[1].clip.w = 2.0f;
    in[1].clip.z = 1.0f;
    CHECK(setupTriangle(in, viewport64(), true, true, screen) == SetupResult::Ok);
    CHECK_NEAR(screen.invW[0], 1.0f, 1e-6);
    CHECK_NEAR(screen.invW[1], 0.5f, 1e-6);
    // NDC z = z/w = 0.5, so window depth = 0.5 * 0.5 + 0.5 = 0.75.
    CHECK_NEAR(screen.depth[1], 0.75f, 1e-6);
}

NOVA_TEST(TriangleSetup, BoundingBoxIsClampedToViewport) {
    ScreenTriangle screen;
    // A triangle far larger than the 64x64 viewport.
    const ClipVertex in[3] = {clipVertex(-40.0f, -40.0f, 0.0f), clipVertex(40.0f, -40.0f, 0.0f),
                              clipVertex(0.0f, 40.0f, 0.0f)};
    CHECK(setupTriangle(in, viewport64(), true, true, screen) == SetupResult::Ok);
    CHECK_EQ(screen.minX, 0);
    CHECK_EQ(screen.minY, 0);
    CHECK_EQ(screen.maxX, 63);
    CHECK_EQ(screen.maxY, 63);
}
