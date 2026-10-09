// NOVA GPU - Software GPU
// examples/lighting_demo.cpp - Phase 4: ambient + directional lighting.
//
// One scene (sphere, cube, floor) rendered with four lighting setups:
//   lighting_lambert.ppm    ambient + directional diffuse
//   lighting_phong.ppm      adds a Blinn-Phong specular highlight
//   lighting_backlit.ppm    light behind the objects: ambient only
//   lighting_colored.ppm    blue ambient + orange key light
//
// The sphere matters here: smooth per-vertex normals make the diffuse falloff
// and the highlight visible as gradients instead of flat faces.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "camera/camera.h"
#include "core/config.h"
#include "debug/overlay.h"
#include "debug/report.h"
#include "gpu/device.h"
#include "lighting/light.h"
#include "math/math_utils.h"
#include "platform/console.h"
#include "platform/ppm_writer.h"
#include "scene/shapes.h"
#include "scene/transform.h"
#include "shader/lambert_shader.h"
#include "shader/phong_shader.h"

using namespace nova;

namespace {

struct Options {
    int width = 640;
    int height = 480;
    int threads = 0;
    int tileSize = 32;
    bool overlay = false;
    std::string prefix = "lighting";
};

void printUsage() {
    std::printf(
        "NOVA GPU lighting demo (Phase 4)\n"
        "  --width N       framebuffer width  (default 640)\n"
        "  --height N      framebuffer height (default 480)\n"
        "  --threads N     rasterizer worker threads (default: hardware concurrency)\n"
        "  --tile N        tile size in pixels (default 32)\n"
        "  --overlay       draw the debug HUD into the images\n"
        "  --prefix NAME   output file prefix (default lighting)\n");
}

struct Variant {
    const char* suffix;
    lighting::AmbientLight ambient;
    lighting::DirectionalLight light;
    lighting::Material sphereMaterial;
    float sphereMaterialShininess;  // unused; kept for clarity of the table below
    lighting::Material floorMaterial;
};

}  // namespace

int main(int argc, char** argv) {
    platform::enableAnsiColors();

    Options options;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const bool hasNext = (i + 1) < argc;
        if (std::strcmp(arg, "--help") == 0) {
            printUsage();
            return 0;
        } else if (std::strcmp(arg, "--width") == 0 && hasNext) {
            options.width = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--height") == 0 && hasNext) {
            options.height = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--threads") == 0 && hasNext) {
            options.threads = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--tile") == 0 && hasNext) {
            options.tileSize = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--overlay") == 0) {
            options.overlay = true;
        } else if (std::strcmp(arg, "--prefix") == 0 && hasNext) {
            options.prefix = argv[++i];
        } else {
            std::fprintf(stderr, "unknown argument: %s (try --help)\n", arg);
            return 2;
        }
    }

    core::RenderConfig config;
    config.width = options.width;
    config.height = options.height;
    config.tileSize = options.tileSize;
    config.threadCount = options.threads;
    config.backfaceCulling = true;

    gpu::Device device(config);

    // --- scene ---------------------------------------------------------------
    const scene::Mesh sphere = scene::makeSphere(1.15f, 48, 24);
    const scene::Mesh cube = scene::makeCube(1.5f);
    const scene::Mesh floor = scene::makeQuad(14.0f, 14.0f, {1.0f, 1.0f, 1.0f});

    const math::Mat4 sphereModel = scene::makeTransform({-1.35f, 0.1f, 0.0f});
    const math::Mat4 cubeModel =
        scene::makeTransform({1.45f, 0.05f, 0.0f}, {math::radians(18.0f), math::radians(32.0f), 0.0f});
    const math::Mat4 floorModel =
        math::translation({0.0f, -1.2f, 0.0f}) * math::rotationX(math::radians(-90.0f));

    camera::Camera cam;
    cam.setPerspective(45.0f, static_cast<float>(options.width) / static_cast<float>(options.height),
                       0.1f, 100.0f);
    cam.setPosition({0.0f, 2.1f, 5.4f});
    cam.setTarget({0.0f, -0.15f, 0.0f});
    device.setCameraPosition(cam.position());

    // --- lighting variants ---------------------------------------------------
    auto keyLight = [](const math::Vec3& direction, const math::Vec3& color) {
        lighting::DirectionalLight light;
        light.direction = math::normalize(direction);
        light.color = color;
        light.intensity = 1.0f;
        return light;
    };
    auto ambientOf = [](const math::Vec3& color, float intensity) {
        lighting::AmbientLight ambient;
        ambient.color = color;
        ambient.intensity = intensity;
        return ambient;
    };

    lighting::Material sphereMaterial;
    sphereMaterial.albedo = {0.82f, 0.85f, 0.92f};
    sphereMaterial.specular = 0.0f;

    lighting::Material cubeMaterial;
    cubeMaterial.albedo = {0.85f, 0.55f, 0.35f};
    cubeMaterial.specular = 0.0f;

    lighting::Material floorMaterial;
    floorMaterial.albedo = {0.55f, 0.58f, 0.62f};
    floorMaterial.specular = 0.0f;

    const Variant variants[4] = {
        {"lambert", ambientOf({1.0f, 1.0f, 1.0f}, 0.18f), keyLight({-0.45f, -0.75f, -0.48f}, {1.0f, 0.97f, 0.92f}),
         sphereMaterial, 0.0f, floorMaterial},
        {"phong", ambientOf({1.0f, 1.0f, 1.0f}, 0.18f), keyLight({-0.45f, -0.75f, -0.48f}, {1.0f, 0.97f, 0.92f}),
         sphereMaterial, 0.0f, floorMaterial},
        {"backlit", ambientOf({0.85f, 0.9f, 1.0f}, 0.30f), keyLight({0.35f, -0.45f, 0.82f}, {1.0f, 1.0f, 1.0f}),
         sphereMaterial, 0.0f, floorMaterial},
        {"colored", ambientOf({0.25f, 0.45f, 0.95f}, 0.55f), keyLight({0.75f, -0.5f, -0.4f}, {1.0f, 0.55f, 0.2f}),
         sphereMaterial, 0.0f, floorMaterial},
    };

    debug::FrameReportContext context;
    context.tileSize = device.tileSize();
    context.tilesX = device.tilesX();
    context.tilesY = device.tilesY();
    context.meshBytes = sphere.memoryBytes() + cube.memoryBytes() + floor.memoryBytes();

    std::printf("NOVA GPU lighting demo (Phase 4)\n");
    std::printf("  sphere: %d triangles, cube: %d triangles\n\n",
                static_cast<int>(sphere.triangleCount()), static_cast<int>(cube.triangleCount()));

    for (const Variant& variant : variants) {
        device.setAmbient(variant.ambient);
        device.setLight(variant.light);

        // "phong" turns the specular highlight on; everything else stays matte.
        const bool specular = std::strcmp(variant.suffix, "phong") == 0;
        lighting::Material sphereMat = variant.sphereMaterial;
        sphereMat.specular = specular ? 0.55f : 0.0f;
        sphereMat.shininess = 48.0f;
        lighting::Material cubeMat = cubeMaterial;
        cubeMat.specular = specular ? 0.35f : 0.0f;
        cubeMat.shininess = 24.0f;

        const shader::PhongFragmentShader sphereShader(sphereMat);
        const shader::PhongFragmentShader cubeShader(cubeMat);
        const shader::PhongFragmentShader floorShader(variant.floorMaterial);

        device.beginFrame(core::kDarkGray);
        device.draw(sphere, sphereModel, cam.viewProjection(), sphereShader);
        device.draw(cube, cubeModel, cam.viewProjection(), cubeShader);
        device.draw(floor, floorModel, cam.viewProjection(), floorShader);
        device.endFrame();

        if (options.overlay) {
            debug::Overlay overlay(device.framebuffer(), 2);
            overlay.drawHud(device.stats());
        }

        const std::string path = options.prefix + "_" + variant.suffix + ".ppm";
        if (!platform::writePpm(path, device.framebuffer())) {
            std::fprintf(stderr, "failed to write %s\n", path.c_str());
            return 1;
        }

        std::printf("%-8s frame=%6.2f ms  raster=%6.2f ms  tris=%4llu  frags=%7llu -> %s\n",
                    variant.suffix, device.stats().timings.frameMs,
                    device.stats().timings.rasterMs,
                    static_cast<unsigned long long>(device.stats().counters.trianglesRasterized),
                    static_cast<unsigned long long>(device.stats().counters.fragmentsShaded),
                    path.c_str());
    }

    context.workingSetBytes = device.workingSetBytes();
    std::printf("\n");
    debug::printStats(device.stats(), context);
    return 0;
}
