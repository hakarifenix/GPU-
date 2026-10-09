// NOVA GPU - Software GPU
// tests/test_frustum.cpp - Phase 5: bounds, frustum extraction, object culling.
#include "test_framework.h"
#include "test_helpers.h"

#include "camera/camera.h"
#include "core/config.h"
#include "gpu/device.h"
#include "math/bounds.h"
#include "math/math_utils.h"
#include "pipeline/frustum.h"
#include "scene/shapes.h"
#include "scene/transform.h"
#include "shader/color_shader.h"

using namespace nova;
using namespace nova::math;
using namespace nova::pipeline;

namespace {

core::RenderConfig testConfig(int width, int height, bool culling) {
    core::RenderConfig config;
    config.width = width;
    config.height = height;
    config.tileSize = 8;
    config.enableThreads = false;
    config.frustumCulling = culling;
    return config;
}

camera::Camera testCamera() {
    camera::Camera cam;
    cam.setPerspective(60.0f, 1.0f, 0.5f, 100.0f);
    cam.setPosition({0.0f, 0.0f, 10.0f});
    cam.setTarget({0.0f, 0.0f, 0.0f});
    return cam;
}

}  // namespace

NOVA_TEST(Bounds, AabbExpandsAndMeasures) {
    Aabb box;
    CHECK(!box.valid());

    box.expand({-1.0f, -2.0f, -3.0f});
    box.expand({1.0f, 2.0f, 3.0f});
    CHECK(box.valid());
    CHECK_NEAR(box.center().x, 0.0f, 1e-6);
    CHECK_NEAR(box.extent().z, 6.0f, 1e-6);
    CHECK_NEAR(box.radius(), length(Vec3{2.0f, 4.0f, 6.0f}) * 0.5f, 1e-5);

    // Expanding with invalid boxes and merging boxes both behave.
    box.expand(Aabb{});
    CHECK(box.valid());
    Aabb other;
    other.expand({5.0f, 5.0f, 5.0f});
    box.expand(other);
    CHECK_NEAR(box.max.x, 5.0f, 1e-6);
}

NOVA_TEST(Bounds, TransformedBoxContainsRotatedGeometry) {
    Aabb box;
    box.expand({-1.0f, -1.0f, -1.0f});
    box.expand({1.0f, 1.0f, 1.0f});

    // Rotating a cube cannot grow its extent beyond the corner radius.
    const Aabb rotated = box.transformed(rotationY(radians(45.0f)));
    CHECK(rotated.valid());
    CHECK(rotated.max.x >= 1.414f - 1e-3f);
    CHECK(rotated.max.x <= 1.414f + 1e-3f);

    // Transformed corner points must stay inside the fitted box.
    const Mat4 m = translation({2.0f, 0.0f, 0.0f}) * rotationZ(radians(30.0f));
    const Aabb fitted = box.transformed(m);
    for (int c = 0; c < 8; ++c) {
        const Vec3 corner{(c & 1) ? box.max.x : box.min.x, (c & 2) ? box.max.y : box.min.y,
                          (c & 4) ? box.max.z : box.min.z};
        const Vec3 p = transformPoint(m, corner);
        CHECK(p.x >= fitted.min.x - 1e-5f && p.x <= fitted.max.x + 1e-5f);
        CHECK(p.y >= fitted.min.y - 1e-5f && p.y <= fitted.max.y + 1e-5f);
        CHECK(p.z >= fitted.min.z - 1e-5f && p.z <= fitted.max.z + 1e-5f);
    }
}

NOVA_TEST(Frustum, ExtractedPlanesClassifyTheViewVolume) {
    const camera::Camera cam = testCamera();
    const Frustum frustum = Frustum::fromViewProjection(cam.viewProjection());

    // A small box at the target, 10 units in front, is visible.
    Aabb center;
    center.expand({-1.0f, -1.0f, -11.0f});
    center.expand({1.0f, 1.0f, -9.0f});
    CHECK(frustum.intersects(center));

    // Fully behind the camera (which sits at z = +10 looking towards -Z).
    // A box spanning [9, 11] would straddle the camera and is correctly kept.
    Aabb behind;
    behind.expand({-1.0f, -1.0f, 11.0f});
    behind.expand({1.0f, 1.0f, 13.0f});
    CHECK(!frustum.intersects(behind));

    Aabb straddlingCamera;
    straddlingCamera.expand({-1.0f, -1.0f, 9.0f});
    straddlingCamera.expand({1.0f, 1.0f, 11.0f});
    CHECK(frustum.intersects(straddlingCamera));

    // Far beyond the far plane (100 units).
    Aabb farAway;
    farAway.expand({-1.0f, -1.0f, -400.0f});
    farAway.expand({1.0f, 1.0f, -300.0f});
    CHECK(!frustum.intersects(farAway));

    // Way off to the side at the same depth as the target.
    Aabb toTheSide;
    toTheSide.expand({300.0f, -1.0f, -11.0f});
    toTheSide.expand({320.0f, 1.0f, -9.0f});
    CHECK(!frustum.intersects(toTheSide));

    // A box straddling the left plane must be kept (conservative test).
    Aabb straddling;
    straddling.expand({-20.0f, -1.0f, -11.0f});
    straddling.expand({0.0f, 1.0f, -9.0f});
    CHECK(frustum.intersects(straddling));

    // The bounding sphere test agrees with the box test here.
    CHECK(frustum.intersectsSphere(Vec3{0.0f, 0.0f, -10.0f}, 1.0f));
    CHECK(!frustum.intersectsSphere(Vec3{0.0f, 0.0f, 10.0f}, 1.0f));
    CHECK(!frustum.intersectsSphere(Vec3{500.0f, 0.0f, -10.0f}, 1.0f));
}

NOVA_TEST(Scene, MeshBoundsMatchTheVertices) {
    const scene::Mesh cube = scene::makeCube(2.0f);
    const math::Aabb& box = cube.bounds();
    CHECK(box.valid());
    CHECK_NEAR(box.min.x, -1.0f, 1e-5);
    CHECK_NEAR(box.max.y, 1.0f, 1e-5);
    CHECK_NEAR(box.radius(), length(Vec3{2.0f, 2.0f, 2.0f}) * 0.5f, 1e-4);

    // The cache must survive repeated queries and reset on demand.
    const math::Aabb& again = cube.bounds();
    CHECK(&again == &box);
    scene::Mesh mutableCube = cube;
    mutableCube.vertices[0].position = {100.0f, 0.0f, 0.0f};
    mutableCube.invalidateBounds();
    CHECK(mutableCube.bounds().max.x >= 100.0f);
}

NOVA_TEST(Frustum, OffscreenObjectIsCulledWithoutRendering) {
    const scene::Mesh quad = novatest::makeQuad(0.5f, 0.5f, 0.0f, {1.0f, 1.0f, 1.0f});
    const camera::Camera cam = testCamera();
    const shader::ColorFragmentShader shader;

    // A quad at x = +50 with a 60-degree field of view is far outside the frustum.
    const Mat4 offscreen = scene::makeTransform({50.0f, 0.0f, -10.0f});

    gpu::Device culled(testConfig(64, 64, true));
    culled.beginFrame(core::kBlack);
    culled.draw(quad, offscreen, cam.viewProjection(), shader);
    culled.endFrame();

    CHECK(novatest::allPixelsAre(culled.framebuffer(), core::kBlack));
    CHECK_EQ(static_cast<int>(culled.stats().counters.objectsSubmitted), 1);
    CHECK_EQ(static_cast<int>(culled.stats().counters.objectsCulled), 1);
    CHECK_EQ(static_cast<int>(culled.stats().counters.trianglesFrustumCulled), 2);
    CHECK_EQ(static_cast<int>(culled.stats().counters.trianglesRasterized), 0);
    CHECK_EQ(static_cast<int>(culled.stats().counters.verticesProcessed), 0);

    // With culling disabled the same draw goes through the whole pipeline and
    // still produces no pixels (the triangles are off-screen), which is the work
    // the visibility test just saved.
    gpu::Device uncullable(testConfig(64, 64, false));
    uncullable.beginFrame(core::kBlack);
    uncullable.draw(quad, offscreen, cam.viewProjection(), shader);
    uncullable.endFrame();
    CHECK(novatest::allPixelsAre(uncullable.framebuffer(), core::kBlack));
    CHECK_EQ(static_cast<int>(uncullable.stats().counters.objectsCulled), 0);
    CHECK_EQ(static_cast<int>(uncullable.stats().counters.verticesProcessed), 4);
}

NOVA_TEST(Frustum, CullingNeverChangesTheRenderedImage) {
    // A scene with both visible and invisible objects must render identically
    // with culling on and off: culling may only remove work, never pixels.
    const scene::Mesh quad = novatest::makeQuad(0.5f, 0.5f, 0.0f, {0.9f, 0.3f, 0.1f});
    const camera::Camera cam = testCamera();
    const shader::ColorFragmentShader shader;

    const Mat4 models[4] = {
        scene::makeTransform({-1.5f, 0.0f, -10.0f}, {0.0f, 0.6f, 0.0f}),
        scene::makeTransform({1.0f, 0.5f, -9.0f}, {0.0f, -0.4f, 0.2f}),
        scene::makeTransform({40.0f, 0.0f, -10.0f}),   // outside
        scene::makeTransform({0.0f, 0.0f, 60.0f}),     // behind the camera
    };

    gpu::Device withCulling(testConfig(96, 96, true));
    gpu::Device withoutCulling(testConfig(96, 96, false));

    for (int frame = 0; frame < 2; ++frame) {
        gpu::Device& a = frame == 0 ? withCulling : withoutCulling;
        a.beginFrame(core::kDarkGray);
        for (const Mat4& model : models) a.draw(quad, model, cam.viewProjection(), shader);
        a.endFrame();
    }

    int mismatches = 0;
    for (size_t i = 0; i < withCulling.framebuffer().pixelCount(); ++i) {
        if (withCulling.framebuffer().colorData()[i] != withoutCulling.framebuffer().colorData()[i]) {
            ++mismatches;
        }
    }
    CHECK_EQ(mismatches, 0);
    CHECK(novatest::countPixels(withCulling.framebuffer(), core::kDarkGray) <
          withCulling.framebuffer().pixelCount());

    CHECK_EQ(static_cast<int>(withCulling.stats().counters.objectsCulled), 2);
    CHECK_EQ(static_cast<int>(withoutCulling.stats().counters.objectsCulled), 0);
    CHECK_EQ(static_cast<int>(withCulling.stats().counters.trianglesRasterized),
             static_cast<int>(withoutCulling.stats().counters.trianglesRasterized));
}
