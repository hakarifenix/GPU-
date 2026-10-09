// NOVA GPU - Software GPU
// tests/test_lighting.cpp - Phase 4: ambient, directional, diffuse, specular.
#include "test_framework.h"
#include "test_helpers.h"

#include <cmath>
#include <set>

#include "camera/camera.h"
#include "core/config.h"
#include "gpu/device.h"
#include "lighting/lighting.h"
#include "scene/shapes.h"
#include "shader/lambert_shader.h"
#include "shader/phong_shader.h"

using namespace nova;

namespace {

lighting::Surface surfaceWithNormal(const math::Vec3& normal,
                                    const math::Vec3& view = {0.0f, 0.0f, 1.0f}) {
    lighting::Surface surface;
    surface.normal = normal;
    surface.viewDirection = math::normalize(view);
    return surface;
}

// Light travelling towards -Z, i.e. shining from +Z onto a +Z surface.
lighting::DirectionalLight frontLight(float intensity = 1.0f) {
    lighting::DirectionalLight light;
    light.direction = {0.0f, 0.0f, -1.0f};
    light.color = {1.0f, 1.0f, 1.0f};
    light.intensity = intensity;
    return light;
}

lighting::AmbientLight whiteAmbient(float intensity) {
    lighting::AmbientLight ambient;
    ambient.color = {1.0f, 1.0f, 1.0f};
    ambient.intensity = intensity;
    return ambient;
}

core::RenderConfig testConfig(int size) {
    core::RenderConfig config;
    config.width = size;
    config.height = size;
    config.tileSize = 8;
    config.enableThreads = false;
    return config;
}

// Mean luminance of the drawn (non-background) pixels in a region.
float meanLuminance(const core::Framebuffer& fb, int y0, int y1) {
    double sum = 0.0;
    size_t count = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = 0; x < fb.width(); ++x) {
            const core::Color c = fb.colorData()[static_cast<size_t>(y) * fb.width() + x];
            if (c == core::kBlack) continue;
            sum += 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
            ++count;
        }
    }
    return count > 0 ? static_cast<float>(sum / static_cast<double>(count)) : 0.0f;
}

}  // namespace

NOVA_TEST(Lighting, AmbientOnlyIsAmbientTimesAlbedo) {
    lighting::Material material;
    material.albedo = {1.0f, 1.0f, 1.0f};

    const math::Vec3 c = lighting::shade(material, whiteAmbient(0.25f), frontLight(0.0f),
                                         surfaceWithNormal({0.0f, 0.0f, 1.0f}));
    CHECK_NEAR(c.x, 0.25f, 1e-5);
    CHECK_NEAR(c.y, 0.25f, 1e-5);
    CHECK_NEAR(c.z, 0.25f, 1e-5);

    // Ambient is tinted by the light color and scaled by the material term.
    lighting::AmbientLight blue;
    blue.color = {0.0f, 0.0f, 1.0f};
    blue.intensity = 0.5f;
    material.ambient = 0.5f;
    const math::Vec3 tinted = lighting::shade(material, blue, frontLight(0.0f),
                                              surfaceWithNormal({0.0f, 0.0f, 1.0f}));
    CHECK_NEAR(tinted.x, 0.0f, 1e-6);
    CHECK_NEAR(tinted.z, 1.0f * 0.5f * 0.5f, 1e-6);
}

NOVA_TEST(Lighting, DiffuseIsFullFacingTheLightAndZeroFacingAway) {
    lighting::Material material;
    material.albedo = {0.8f, 0.4f, 0.2f};

    const math::Vec3 facing = lighting::shade(material, whiteAmbient(0.0f), frontLight(),
                                              surfaceWithNormal({0.0f, 0.0f, 1.0f}));
    CHECK_NEAR(facing.x, 0.8f, 1e-5);
    CHECK_NEAR(facing.y, 0.4f, 1e-5);
    CHECK_NEAR(facing.z, 0.2f, 1e-5);

    const math::Vec3 away = lighting::shade(material, whiteAmbient(0.0f), frontLight(),
                                            surfaceWithNormal({0.0f, 0.0f, -1.0f}));
    CHECK_NEAR(away.x, 0.0f, 1e-6);
    CHECK_NEAR(away.z, 0.0f, 1e-6);

    // A side-facing normal is half lit (60 degrees from the light).
    const math::Vec3 side =
        lighting::shade(material, whiteAmbient(0.0f), frontLight(),
                        surfaceWithNormal({0.8660254f, 0.0f, 0.5f}));
    CHECK_NEAR(side.x, 0.8f * 0.5f, 1e-4);
}

NOVA_TEST(Lighting, IntensityAndAlbedoAreLinear) {
    lighting::Material material;
    material.albedo = {0.5f, 0.5f, 0.5f};

    const math::Vec3 half = lighting::shade(material, whiteAmbient(0.2f), frontLight(0.5f),
                                            surfaceWithNormal({0.0f, 0.0f, 1.0f}));
    const math::Vec3 full = lighting::shade(material, whiteAmbient(0.4f), frontLight(1.0f),
                                            surfaceWithNormal({0.0f, 0.0f, 1.0f}));
    CHECK_NEAR(full.x, 2.0f * half.x, 1e-5);

    // material.diffuse scales the directional term only.
    material.diffuse = 0.5f;
    const math::Vec3 reduced = lighting::shade(material, whiteAmbient(0.4f), frontLight(1.0f),
                                               surfaceWithNormal({0.0f, 0.0f, 1.0f}));
    CHECK_NEAR(reduced.x, 0.5f * 0.4f + 0.5f * 0.5f * 1.0f, 1e-5);
}

NOVA_TEST(Lighting, SpecularPeaksAtTheReflectionDirection) {
    lighting::Material material;
    material.albedo = {0.0f, 0.0f, 0.0f};  // isolate the specular term
    material.ambient = 0.0f;
    material.diffuse = 0.0f;
    material.specular = 0.5f;
    material.shininess = 32.0f;

    // Light and view both along +Z: the perfect mirror normal is +Z.
    const math::Vec3 onAxis =
        lighting::shade(material, whiteAmbient(0.0f), frontLight(),
                        surfaceWithNormal({0.0f, 0.0f, 1.0f}));
    CHECK_NEAR(onAxis.x, 0.5f, 1e-4);

    // Tilting the normal by 30 degrees drops the highlight sharply.
    const math::Vec3 tilted =
        lighting::shade(material, whiteAmbient(0.0f), frontLight(),
                        surfaceWithNormal({0.5f, 0.0f, 0.8660254f}));
    CHECK(tilted.x < 0.5f * 0.2f);

    // A material without specular never gets a highlight.
    material.specular = 0.0f;
    const math::Vec3 matte = lighting::shade(material, whiteAmbient(0.0f), frontLight(),
                                             surfaceWithNormal({0.0f, 0.0f, 1.0f}));
    CHECK_NEAR(matte.x, 0.0f, 1e-6);
}

NOVA_TEST(Lighting, SpecularCutoffStaysBelowOneLsb) {
    // shade() skips pow() when the highlight cannot reach one 8-bit step. This
    // pins the boundary: at nDotH = 0.5 a shininess >= 8 highlight is invisible,
    // while a broad highlight (shininess 4) must still be evaluated.
    lighting::Material material;
    material.albedo = {0.0f, 0.0f, 0.0f};
    material.ambient = 0.0f;
    material.diffuse = 0.0f;
    material.specular = 1.0f;

    // Light and view along +Z put the halfway vector on +Z, so tilting the
    // normal by 60 degrees makes dot(N, H) exactly 0.5.
    const math::Vec3 normal{0.8660254f, 0.0f, 0.5f};

    for (float shininess : {8.0f, 32.0f, 64.0f}) {
        material.shininess = shininess;
        const math::Vec3 c = lighting::shade(material, whiteAmbient(0.0f), frontLight(),
                                             surfaceWithNormal(normal));
        CHECK(c.x * 255.0f <= 1.0f);
    }

    material.shininess = 4.0f;
    const math::Vec3 broad = lighting::shade(material, whiteAmbient(0.0f), frontLight(),
                                             surfaceWithNormal(normal));
    CHECK_NEAR(broad.x, 0.0625f, 1e-4);  // 0.5^4, evaluated normally
}

NOVA_TEST(Lighting, SpecularIsSuppressedOnBackFacingSurfaces) {
    lighting::Material material;
    material.specular = 1.0f;
    // Normal facing away from the light: no diffuse, and no highlight either.
    const math::Vec3 c = lighting::shade(material, whiteAmbient(0.0f), frontLight(),
                                         surfaceWithNormal({0.0f, 0.0f, -1.0f}));
    CHECK_NEAR(c.x, 0.0f, 1e-6);
}

NOVA_TEST(Lighting, DegenerateNormalIsSafe) {
    lighting::Material material;
    const math::Vec3 c = lighting::shade(material, whiteAmbient(0.5f), frontLight(),
                                         surfaceWithNormal({0.0f, 0.0f, 0.0f}));
    CHECK(!(c.x != c.x));  // NaN check
    CHECK_NEAR(c.x, 0.5f, 1e-5);  // falls back to ambient
}

NOVA_TEST(Lighting, SphereHasASmoothDiffuseGradientTowardsTheLight) {
    const scene::Mesh sphere = scene::makeSphere(1.0f, 24, 12);
    const shader::LambertFragmentShader fragment;

    camera::Camera cam;
    cam.setPerspective(60.0f, 1.0f, 0.1f, 100.0f);
    cam.setPosition({0.0f, 0.0f, 3.2f});
    cam.setTarget({0.0f, 0.0f, 0.0f});

    gpu::Device device(testConfig(96));
    device.setCameraPosition(cam.position());
    device.setAmbient(whiteAmbient(0.1f));
    lighting::DirectionalLight light = frontLight();
    light.direction = {0.0f, -1.0f, 0.0f};  // straight down
    device.setLight(light);

    device.beginFrame(core::kBlack);
    device.draw(sphere, math::Mat4::identity(), cam.viewProjection(), fragment);
    device.endFrame();

    const core::Framebuffer& fb = device.framebuffer();
    CHECK(novatest::countPixels(fb, core::kBlack) < fb.pixelCount());

    // A smooth shaded sphere must produce many distinct gray levels ...
    std::set<uint8_t> levels;
    for (size_t i = 0; i < fb.pixelCount(); ++i) {
        const core::Color c = fb.colorData()[i];
        if (c != core::kBlack) levels.insert(c.r);
    }
    CHECK(levels.size() > 20);

    // ... and the half facing the light is brighter than the other half.
    const float top = meanLuminance(fb, 0, fb.height() / 2);
    const float bottom = meanLuminance(fb, fb.height() / 2, fb.height());
    CHECK(top > bottom * 1.5f);
}

NOVA_TEST(Lighting, PhongAddsALocalisedSpecularHighlight) {
    const scene::Mesh sphere = scene::makeSphere(1.0f, 32, 16);

    camera::Camera cam;
    cam.setPerspective(50.0f, 1.0f, 0.1f, 100.0f);
    cam.setPosition({0.0f, 0.0f, 4.0f});
    cam.setTarget({0.0f, 0.0f, 0.0f});

    lighting::AmbientLight ambient = whiteAmbient(0.15f);
    lighting::DirectionalLight light;
    light.direction = math::normalize({-0.45f, -0.75f, -0.48f});  // from above-front

    lighting::Material base;
    base.albedo = {0.35f, 0.45f, 0.75f};

    shader::PhongFragmentShader matte(base);  // specular = 0
    lighting::Material shiny = base;
    shiny.specular = 0.6f;
    shiny.shininess = 64.0f;
    shader::PhongFragmentShader gloss(shiny);

    gpu::Device device(testConfig(96));
    device.setCameraPosition(cam.position());
    device.setAmbient(ambient);
    device.setLight(light);

    device.beginFrame(core::kBlack);
    device.draw(sphere, math::Mat4::identity(), cam.viewProjection(), matte);
    device.endFrame();
    const size_t matteBright = novatest::countPixelsNear(device.framebuffer(), core::kWhite, 1);

    device.beginFrame(core::kBlack);
    device.draw(sphere, math::Mat4::identity(), cam.viewProjection(), gloss);
    device.endFrame();
    const core::Framebuffer& fb = device.framebuffer();

    // The highlight is bright ...
    int maxLuma = 0;
    size_t bright = 0;
    for (size_t i = 0; i < fb.pixelCount(); ++i) {
        const core::Color c = fb.colorData()[i];
        if (c == core::kBlack) continue;
        const int luma = static_cast<int>(0.299 * c.r + 0.587 * c.g + 0.114 * c.b);
        if (luma > maxLuma) maxLuma = luma;
        if (luma > 230) ++bright;
    }
    CHECK(maxLuma > 240);
    // ... and localised: a small fraction of the sphere, not the whole surface.
    CHECK(bright > 0);
    CHECK(bright < 200);
    CHECK_EQ(static_cast<int>(matteBright), 0);  // a matte surface has no highlight
}
