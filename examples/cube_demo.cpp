// NOVA GPU - Software GPU
// examples/cube_demo.cpp - step 2/3/4 of the Phase 1 plan:
//
//   "Add depth buffer and render a rotating cube."
//   "Add camera and perspective projection."
//   "Add a debug mode."
//
// The cube is transformed by a model matrix, projected by a perspective camera,
// near-plane clipped, triangle-setup, tile-rasterized with the depth test in
// software, and finally written to a PPM image - all on the CPU.
//
// --flythrough moves the camera through the cube, which exercises near-plane
// clipping in a way that a static camera never does.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "camera/camera.h"
#include "math/math_utils.h"
#include "core/config.h"
#include "debug/overlay.h"
#include "debug/report.h"
#include "gpu/device.h"
#include "platform/console.h"
#include "platform/ppm_writer.h"
#include "scene/shapes.h"
#include "scene/transform.h"
#include "shader/color_shader.h"

using namespace nova;

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Options {
    int width = 640;
    int height = 480;
    int threads = 0;
    int tileSize = 32;
    int frames = 1;
    float degreesPerFrame = 2.0f;
    bool overlay = true;
    bool flythrough = false;
    bool animate = false;
    bool debug = false;
    std::string out = "cube.ppm";
};

void printUsage() {
    std::printf(
        "NOVA GPU cube demo\n"
        "  --width N        framebuffer width  (default 640)\n"
        "  --height N       framebuffer height (default 480)\n"
        "  --threads N      rasterizer worker threads (default: hardware concurrency)\n"
        "  --tile N         tile size in pixels (default 32)\n"
        "  --frames N       number of frames to render (default 1)\n"
        "  --degrees N      degrees of rotation per frame (default 2)\n"
        "  --flythrough     move the camera through the cube (exercises clipping)\n"
        "  --animate        write cube_0000.ppm, cube_0001.ppm, ...\n"
        "  --no-overlay     do not draw the debug HUD into the image\n"
        "  --debug          print the frame statistics to the console every frame\n"
        "  --out PATH       output path for the final frame (default cube.ppm)\n");
}

// Writes "<stem>_%04d.ppm" next to the final-frame output.
std::string animationPath(const std::string& out, int frame) {
    const size_t slash = out.find_last_of("/\\");
    const size_t dot = out.find_last_of('.');
    const std::string stem = (dot != std::string::npos && (slash == std::string::npos || dot > slash))
                                 ? out.substr(0, dot)
                                 : out;
    char suffix[32];
    std::snprintf(suffix, sizeof(suffix), "_%04d.ppm", frame);
    return stem + suffix;
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
        } else if (std::strcmp(arg, "--frames") == 0 && hasNext) {
            options.frames = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--degrees") == 0 && hasNext) {
            options.degreesPerFrame = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(arg, "--flythrough") == 0) {
            options.flythrough = true;
        } else if (std::strcmp(arg, "--animate") == 0) {
            options.animate = true;
        } else if (std::strcmp(arg, "--no-overlay") == 0) {
            options.overlay = false;
        } else if (std::strcmp(arg, "--debug") == 0) {
            options.debug = true;
        } else if (std::strcmp(arg, "--out") == 0 && hasNext) {
            options.out = argv[++i];
        } else {
            std::fprintf(stderr, "unknown argument: %s (try --help)\n", arg);
            return 2;
        }
    }

    if (options.frames < 1) options.frames = 1;

    core::RenderConfig config;
    config.width = options.width;
    config.height = options.height;
    config.tileSize = options.tileSize;
    config.threadCount = options.threads;
    config.backfaceCulling = true;
    config.depthTest = true;

    gpu::Device device(config);
    const scene::Mesh cube = scene::makeCube(1.6f);
    const shader::ColorFragmentShader shader;

    camera::Camera cam;
    cam.setPerspective(60.0f,
                       static_cast<float>(options.width) / static_cast<float>(options.height), 0.1f,
                       100.0f);
    cam.setPosition({0.0f, 0.0f, 4.0f});
    cam.setTarget({0.0f, 0.0f, 0.0f});

    debug::Overlay overlay(device.framebuffer(), 2);
    debug::FrameReportContext context;
    context.tileSize = device.tileSize();
    context.tilesX = device.tilesX();
    context.tilesY = device.tilesY();
    context.meshBytes = cube.memoryBytes();

    const std::string consoleOut = options.out;
    int written = 0;

    // A fixed starting pose so that the very first frame already shows the cube
    // as a 3D object instead of a face-on square.
    const float baseX = math::radians(26.0f);
    const float baseY = math::radians(37.0f);

    for (int frame = 0; frame < options.frames; ++frame) {
        const float angle = math::radians(options.degreesPerFrame) * static_cast<float>(frame);
        const math::Mat4 model = scene::makeTransform(
            {0.0f, 0.0f, 0.0f}, {baseX + angle * 0.37f, baseY + angle, angle * 0.23f});

        if (options.flythrough) {
            // Keep the view direction fixed and slide the eye along -Z so the
            // geometry passes through the near plane.
            const float t = options.frames > 1
                                ? static_cast<float>(frame) / static_cast<float>(options.frames - 1)
                                : 0.0f;
            const math::Vec3 eye{0.0f, 0.55f, 4.0f - 8.0f * t};
            cam.setPosition(eye);
            cam.setTarget(eye + math::Vec3{0.0f, 0.0f, -1.0f});
        }

        const bool lastFrame = (frame + 1 == options.frames);

        device.beginFrame(core::kDarkGray);
        device.draw(cube, model, cam.viewProjection(), shader);
        device.endFrame();

        if (options.overlay) overlay.drawHud(device.stats());
        if (options.debug) debug::printStatsLine(device.stats());

        // Write the final frame, and every frame when animating.
        const bool shouldWrite = lastFrame || options.animate;
        if (shouldWrite) {
            const std::string path = (options.animate && !lastFrame) ? animationPath(options.out, frame)
                                                                    : options.out;
            if (!platform::writePpm(path, device.framebuffer())) {
                std::fprintf(stderr, "failed to write %s\n", path.c_str());
                return 1;
            }
            ++written;
        }
    }

    context.workingSetBytes = device.workingSetBytes();

    if (options.debug) std::printf("\n");
    std::printf("rendered %d frame(s), wrote %d image(s) [%s]\n\n", options.frames, written,
                consoleOut.c_str());
    debug::printStats(device.stats(), context);
    return 0;
}
