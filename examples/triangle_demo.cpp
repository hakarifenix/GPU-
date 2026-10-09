// NOVA GPU - Software GPU
// examples/triangle_demo.cpp - step 1 of the Phase 1 plan:
//
//   "Create the project. Build successfully. Render a colored triangle using
//    the CPU."
//
// One world-space triangle is transformed by a perspective camera, clipped,
// set up, rasterized on the CPU and written to a PPM image. No GPU is involved
// at any point: every pixel below is written by nova::rasterizer::Rasterizer.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "camera/camera.h"
#include "core/config.h"
#include "debug/overlay.h"
#include "debug/report.h"
#include "gpu/device.h"
#include "platform/console.h"
#include "platform/ppm_writer.h"
#include "scene/mesh.h"
#include "shader/color_shader.h"

using namespace nova;

namespace {

struct Options {
    int width = 640;
    int height = 480;
    int threads = 0;  // 0 = hardware concurrency
    int tileSize = 32;
    bool overlay = true;
    bool verboseHud = false;
    std::string out = "triangle.ppm";
};

void printUsage() {
    std::printf(
        "NOVA GPU triangle demo\n"
        "  --width N     framebuffer width  (default 640)\n"
        "  --height N    framebuffer height (default 480)\n"
        "  --threads N   rasterizer worker threads (default: hardware concurrency)\n"
        "  --tile N      tile size in pixels (default 32)\n"
        "  --no-overlay  do not draw the debug HUD into the image\n"
        "  --verbose-hud    extended HUD: shaders, memory and frame-time graph\n"
        "  --out PATH    output PPM path (default triangle.ppm)\n");
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
        } else if (std::strcmp(arg, "--no-overlay") == 0) {
            options.overlay = false;
        } else if (std::strcmp(arg, "--verbose-hud") == 0) {
            options.verboseHud = true;
        } else if (std::strcmp(arg, "--out") == 0 && hasNext) {
            options.out = argv[++i];
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
    config.backfaceCulling = false;  // a single triangle is shown from both sides

    gpu::Device device(config);

    // --- geometry ------------------------------------------------------------
    // A triangle in the z = 0 plane, wound counter-clockwise as seen from +Z.
    scene::Mesh mesh;
    const math::Vec3 positions[3] = {{-1.0f, -0.8f, 0.0f}, {1.0f, -0.8f, 0.0f}, {0.0f, 0.9f, 0.0f}};
    const math::Vec3 colors[3] = {{1.0f, 0.25f, 0.25f}, {0.25f, 1.0f, 0.35f}, {0.35f, 0.55f, 1.0f}};
    for (int i = 0; i < 3; ++i) {
        core::Vertex vertex;
        vertex.position = positions[i];
        vertex.normal = {0.0f, 0.0f, 1.0f};
        vertex.color = colors[i];  // interpolated by the rasterizer (Gouraud)
        vertex.uv = {0.0f, 0.0f};
        mesh.vertices.push_back(vertex);
    }
    mesh.addTriangle(0, 1, 2);

    // --- camera --------------------------------------------------------------
    camera::Camera cam;
    cam.setPerspective(60.0f, static_cast<float>(options.width) / static_cast<float>(options.height),
                       0.1f, 100.0f);
    cam.setPosition({0.0f, 0.0f, 3.0f});
    cam.setTarget({0.0f, 0.0f, 0.0f});

    const shader::ColorFragmentShader shader;

    device.beginFrame(core::kDarkGray);
    device.draw(mesh, math::Mat4::identity(), cam.viewProjection(), shader);
    device.endFrame();

    if (options.overlay) {
        debug::Overlay overlay(device.framebuffer(), 2);
        overlay.drawHud(device.stats(), 8, 8, options.verboseHud);
    }

    if (!platform::writePpm(options.out, device.framebuffer())) {
        std::fprintf(stderr, "failed to write %s\n", options.out.c_str());
        return 1;
    }

    debug::FrameReportContext context;
    context.tileSize = device.tileSize();
    context.tilesX = device.tilesX();
    context.tilesY = device.tilesY();
    context.workingSetBytes = device.workingSetBytes();
    context.meshBytes = mesh.memoryBytes();

    std::printf("wrote %s (%dx%d)\n\n", options.out.c_str(), device.framebuffer().width(),
                device.framebuffer().height());
    debug::printStats(device.stats(), context);
    return 0;
}
