// NOVA GPU - Software GPU
// examples/texture_demo.cpp - Phase 3: texture sampling in the fragment stage.
//
// Four images from one scene, differing only in the shader / sampler state:
//   texture_nearest.ppm   file texture, nearest filtering, uv repeated 3x
//   texture_bilinear.ppm  the same with bilinear filtering
//   texture_uvgrid.ppm    uv-grid texture, so the uv layout is readable by eye
//   texture_lit.ppm       texture as albedo -> Phase 4 lighting (Phong)
//
// The texture is loaded from a PPM file when one is available and falls back to
// a procedurally generated image, so the demo runs from any working directory.
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
#include "shader/phong_shader.h"
#include "shader/texture_shader.h"
#include "texture/image_io.h"
#include "texture/texture.h"

using namespace nova;

namespace {

struct Options {
    int width = 640;
    int height = 480;
    int threads = 0;
    int tileSize = 32;
    bool overlay = false;
    std::string prefix = "texture";
    std::string texturePath = "assets/nova_checker.ppm";
    std::string gridPath = "assets/nova_uv.ppm";
};

void printUsage() {
    std::printf(
        "NOVA GPU texture demo (Phase 3)\n"
        "  --width N       framebuffer width  (default 640)\n"
        "  --height N      framebuffer height (default 480)\n"
        "  --threads N     rasterizer worker threads (default: hardware concurrency)\n"
        "  --tile N        tile size in pixels (default 32)\n"
        "  --texture PATH  PPM texture to load (default assets/nova_checker.ppm)\n"
        "  --grid PATH     uv-grid PPM (default assets/nova_uv.ppm)\n"
        "  --overlay       draw the debug HUD into the images\n"
        "  --prefix NAME   output file prefix (default texture)\n");
}

// Screen-aligned quad whose uv range is scaled, so the sampler's wrap mode and
// filtering are both visible. `uRepeat`/`vRepeat` are in texture units.
scene::Mesh makeUvQuad(float halfWidth, float halfHeight, float uRepeat, float vRepeat,
                       const math::Vec3& color) {
    scene::Mesh mesh;
    const math::Vec3 corners[4] = {{-halfWidth, -halfHeight, 0.0f},
                                   {halfWidth, -halfHeight, 0.0f},
                                   {halfWidth, halfHeight, 0.0f},
                                   {-halfWidth, halfHeight, 0.0f}};
    const float uv[4][2] = {{0.0f, 0.0f}, {uRepeat, 0.0f}, {uRepeat, vRepeat}, {0.0f, vRepeat}};
    for (int i = 0; i < 4; ++i) {
        core::Vertex v;
        v.position = corners[i];
        v.normal = {0.0f, 0.0f, 1.0f};
        v.color = color;
        v.uv = {uv[i][0], uv[i][1]};
        mesh.vertices.push_back(v);
    }
    mesh.addTriangle(0, 1, 2);
    mesh.addTriangle(0, 2, 3);
    return mesh;
}

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
        } else if (std::strcmp(arg, "--texture") == 0 && hasNext) {
            options.texturePath = argv[++i];
        } else if (std::strcmp(arg, "--grid") == 0 && hasNext) {
            options.gridPath = argv[++i];
        } else if (std::strcmp(arg, "--overlay") == 0) {
            options.overlay = true;
        } else if (std::strcmp(arg, "--prefix") == 0 && hasNext) {
            options.prefix = argv[++i];
        } else {
            std::fprintf(stderr, "unknown argument: %s (try --help)\n", arg);
            return 2;
        }
    }

    // --- textures -------------------------------------------------------------
    texture::Texture loaded = texture::loadPpm(options.texturePath);
    const bool fromFile = !loaded.empty();
    if (!fromFile) {
        loaded = texture::Texture::checker(64, 64, 8, core::Color{235, 235, 245, 255},
                                           core::Color{40, 60, 140, 255});
    }
    texture::Texture grid = texture::loadPpm(options.gridPath);
    const bool gridFromFile = !grid.empty();
    if (!gridFromFile) grid = texture::Texture::uvGrid(64, 64, 8);

    std::printf("NOVA GPU texture demo (Phase 3)\n");
    std::printf("  albedo texture : %s (%dx%d)\n",
                fromFile ? options.texturePath.c_str() : "procedural checker", loaded.width(),
                loaded.height());
    std::printf("  uv-grid texture: %s (%dx%d)\n",
                gridFromFile ? options.gridPath.c_str() : "procedural uv grid", grid.width(),
                grid.height());

    core::RenderConfig config;
    config.width = options.width;
    config.height = options.height;
    config.tileSize = options.tileSize;
    config.threadCount = options.threads;
    config.backfaceCulling = true;

    gpu::Device device(config);

    // 3x3 texture repeat across a screen-aligned quad (identity matrices: the
    // vertex positions are already NDC).
    const scene::Mesh tiledQuad = makeUvQuad(0.85f, 0.85f, 3.0f, 3.0f, {1.0f, 1.0f, 1.0f});
    const scene::Mesh gridQuad = makeUvQuad(0.85f, 0.85f, 1.0f, 1.0f, {1.0f, 1.0f, 1.0f});

    texture::Sampler nearest;
    nearest.filter = texture::SampleMode::Nearest;
    nearest.wrap = texture::WrapMode::Repeat;
    texture::Sampler bilinear;
    bilinear.filter = texture::SampleMode::Bilinear;
    bilinear.wrap = texture::WrapMode::Repeat;

    shader::TexturedFragmentShader nearestShader(loaded, nearest);
    shader::TexturedFragmentShader bilinearShader(loaded, bilinear);
    shader::TexturedFragmentShader gridShader(grid, bilinear);

    debug::FrameReportContext context;
    context.tileSize = device.tileSize();
    context.tilesX = device.tilesX();
    context.tilesY = device.tilesY();
    context.meshBytes = tiledQuad.memoryBytes();

    // --- screen-space comparisons --------------------------------------------
    struct ScreenVariant {
        const char* suffix;
        const scene::Mesh* mesh;
        const shader::FragmentShader* shader;
    };
    const ScreenVariant screenVariants[3] = {
        {"nearest", &tiledQuad, &nearestShader},
        {"bilinear", &tiledQuad, &bilinearShader},
        {"uvgrid", &gridQuad, &gridShader},
    };

    for (const ScreenVariant& variant : screenVariants) {
        device.beginFrame(core::kDarkGray);
        device.draw(*variant.mesh, math::Mat4::identity(), math::Mat4::identity(),
                    *variant.shader);
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
        std::printf("%-9s frame=%6.2f ms  raster=%6.2f ms  frags=%7llu -> %s\n", variant.suffix,
                    device.stats().timings.frameMs, device.stats().timings.rasterMs,
                    static_cast<unsigned long long>(device.stats().counters.fragmentsShaded),
                    path.c_str());
    }

    // --- phase 4 integration: textured surfaces under lighting ---------------
    camera::Camera cam;
    cam.setPerspective(45.0f, static_cast<float>(options.width) / static_cast<float>(options.height),
                       0.1f, 100.0f);
    cam.setPosition({2.4f, 1.9f, 4.6f});
    cam.setTarget({0.0f, 0.0f, 0.0f});
    device.setCameraPosition(cam.position());

    lighting::AmbientLight ambient;
    ambient.color = {0.85f, 0.9f, 1.0f};
    ambient.intensity = 0.22f;
    device.setAmbient(ambient);

    lighting::DirectionalLight light;
    light.direction = math::normalize({-0.5f, -0.8f, -0.35f});
    light.color = {1.0f, 0.96f, 0.88f};
    device.setLight(light);

    lighting::Material material;
    material.albedo = {1.0f, 1.0f, 1.0f};
    material.specular = 0.25f;
    material.shininess = 24.0f;

    shader::PhongFragmentShader litShader(material);
    litShader.setAlbedoTexture(&loaded, bilinear);

    scene::Mesh floor = scene::makeQuad(10.0f, 10.0f, {1.0f, 1.0f, 1.0f});
    // Repeat the texture across the floor so tiling is visible under perspective.
    for (size_t i = 0; i < floor.vertexCount(); ++i) {
        floor.vertices[i].uv = floor.vertices[i].uv * 4.0f;
    }
    const math::Mat4 floorModel =
        math::translation({0.0f, -1.05f, 0.0f}) * math::rotationX(math::radians(-90.0f));
    const scene::Mesh cube = scene::makeCube(1.4f);
    const math::Mat4 cubeModel =
        scene::makeTransform({0.0f, -0.3f, 0.0f}, {math::radians(20.0f), math::radians(35.0f), 0.0f});

    device.beginFrame(core::kDarkGray);
    device.draw(cube, cubeModel, cam.viewProjection(), litShader);
    device.draw(floor, floorModel, cam.viewProjection(), litShader);
    device.endFrame();

    if (options.overlay) {
        debug::Overlay overlay(device.framebuffer(), 2);
        overlay.drawHud(device.stats());
    }
    const std::string litPath = options.prefix + "_lit.ppm";
    if (!platform::writePpm(litPath, device.framebuffer())) {
        std::fprintf(stderr, "failed to write %s\n", litPath.c_str());
        return 1;
    }
    std::printf("%-9s frame=%6.2f ms  raster=%6.2f ms  frags=%7llu -> %s\n", "lit",
                device.stats().timings.frameMs, device.stats().timings.rasterMs,
                static_cast<unsigned long long>(device.stats().counters.fragmentsShaded),
                litPath.c_str());

    context.workingSetBytes = device.workingSetBytes() + loaded.memoryBytes() + grid.memoryBytes();
    std::printf("\n");
    debug::printStats(device.stats(), context);
    return 0;
}
