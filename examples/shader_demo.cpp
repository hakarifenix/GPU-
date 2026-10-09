// NOVA GPU - Software GPU
// examples/shader_demo.cpp - Phase 2: the same scene rendered through different
// shader pairs.
//
// Four images are produced from one scene (cube + floor):
//   shader_color.ppm    TransformVertexShader + ColorFragmentShader   (Phase 1 look)
//   shader_lambert.ppm  TransformVertexShader + LambertFragmentShader (diffuse light)
//   shader_checker.ppm  TransformVertexShader + CheckerFragmentShader (uv driven)
//   shader_wave.ppm     WaveVertexShader      + LambertFragmentShader (custom vertex stage)
//
// Nothing about the pipeline changes between the four: only the shader objects
// passed to Device::draw(). All pixels are still produced by the CPU rasterizer.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "camera/camera.h"
#include "core/config.h"
#include "debug/overlay.h"
#include "debug/report.h"
#include "gpu/device.h"
#include "math/math_utils.h"
#include "platform/console.h"
#include "platform/ppm_writer.h"
#include "scene/shapes.h"
#include "scene/transform.h"
#include "shader/checker_shader.h"
#include "shader/color_shader.h"
#include "shader/lambert_shader.h"
#include "shader/transform_vertex_shader.h"

using namespace nova;

namespace {

// Example application shader: displaces each vertex along its normal with a
// sine wave, using the per-draw time uniform. This is the "author your own
// vertex shader" case the interface has to support.
class WaveVertexShader final : public shader::VertexShader {
public:
    void shade(const core::Vertex& in, const shader::ShaderUniforms& uniforms,
               shader::VertexOutput& out) const override {
        const float phase = 5.0f * (in.position.x + in.position.y + in.position.z + uniforms.time);
        const math::Vec3 displaced = in.position + in.normal * (std::sin(phase) * 0.09f);

        out.clip = math::transform(uniforms.viewProjection * uniforms.model,
                                   math::Vec4{displaced, 1.0f});
        out.varying.worldPosition = math::transformPoint(uniforms.model, displaced);
        out.varying.normal = math::transformDirection(uniforms.model, in.normal);
        out.varying.color = in.color;
        out.varying.uv = in.uv;
    }

    const char* name() const override { return "WaveVertexShader"; }
};

struct Options {
    int width = 640;
    int height = 480;
    int threads = 0;
    int tileSize = 32;
    bool overlay = false;
    bool verboseHud = false;
    std::string prefix = "shader";
};

void printUsage() {
    std::printf(
        "NOVA GPU shader demo (Phase 2)\n"
        "  --width N       framebuffer width  (default 640)\n"
        "  --height N      framebuffer height (default 480)\n"
        "  --threads N     rasterizer worker threads (default: hardware concurrency)\n"
        "  --tile N        tile size in pixels (default 32)\n"
        "  --overlay       draw the debug HUD into the images\n"
        "  --verbose-hud    extended HUD: shaders, memory and frame-time graph\n"
        "  --prefix NAME   output file prefix (default shader)\n");
}

struct Variant {
    const char* suffix;
    const shader::VertexShader* vertexShader;
    const shader::FragmentShader* fragmentShader;
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
        } else if (std::strcmp(arg, "--verbose-hud") == 0) {
            options.verboseHud = true;
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
    const scene::Mesh cube = scene::makeCube(1.6f);
    scene::Mesh floor = scene::makeQuad(8.0f, 8.0f, {1.0f, 1.0f, 1.0f});
    const math::Mat4 floorModel = math::translation({0.0f, -1.15f, 0.0f}) * math::rotationX(math::radians(-90.0f));

    camera::Camera cam;
    cam.setPerspective(60.0f, static_cast<float>(options.width) / static_cast<float>(options.height),
                       0.1f, 100.0f);
    cam.setPosition({2.6f, 1.8f, 4.2f});
    cam.setTarget({0.0f, -0.1f, 0.0f});

    device.setCameraPosition(cam.position());
    device.setTime(0.0f);

    lighting::AmbientLight ambient;
    ambient.color = {1.0f, 1.0f, 1.0f};
    ambient.intensity = 0.18f;
    device.setAmbient(ambient);

    lighting::DirectionalLight light;
    light.direction = math::normalize({-0.4f, -0.8f, -0.45f});
    light.color = {1.0f, 0.97f, 0.9f};
    light.intensity = 1.0f;
    device.setLight(light);

    const shader::TransformVertexShader transformVertex;
    const shader::ColorFragmentShader colorFragment;
    const shader::LambertFragmentShader lambertFragment;
    const shader::CheckerFragmentShader checkerFragment(2.0f, {0.85f, 0.87f, 0.92f},
                                                        {0.20f, 0.22f, 0.28f});
    const WaveVertexShader waveVertex;

    const Variant variants[4] = {
        {"color", &transformVertex, &colorFragment},
        {"lambert", &transformVertex, &lambertFragment},
        {"checker", &transformVertex, &checkerFragment},
        {"wave", &waveVertex, &lambertFragment},
    };

    // Slightly rotated so every frame shows three faces of the cube.
    const math::Mat4 cubeModel =
        scene::makeTransform({0.0f, 0.0f, 0.0f}, {math::radians(24.0f), math::radians(38.0f), 0.0f});

    debug::FrameReportContext context;
    context.tileSize = device.tileSize();
    context.tilesX = device.tilesX();
    context.tilesY = device.tilesY();
    context.meshBytes = cube.memoryBytes() + floor.memoryBytes();

    std::printf("NOVA GPU shader demo: %d variants, %dx%d, threads=%d, tile=%d\n\n", 4,
                config.width, config.height, device.threadCount(), device.tileSize());

    for (const Variant& variant : variants) {
        device.beginFrame(core::kDarkGray);
        device.draw(cube, cubeModel, cam.viewProjection(), *variant.vertexShader,
                    *variant.fragmentShader);
        device.draw(floor, floorModel, cam.viewProjection(), *variant.vertexShader,
                    *variant.fragmentShader);
        device.setTime(0.7f);  // steady pose for the wave shader
        device.endFrame();

        if (options.overlay) {
            debug::Overlay overlay(device.framebuffer(), 2);
            overlay.drawHud(device.stats(), 8, 8, options.verboseHud);
        }

        const std::string path = options.prefix + "_" + variant.suffix + ".ppm";
        if (!platform::writePpm(path, device.framebuffer())) {
            std::fprintf(stderr, "failed to write %s\n", path.c_str());
            return 1;
        }

        const debug::RenderStats& stats = device.stats();
        std::printf("%-8s vertex=%-16s fragment=%-20s frame=%6.2f ms  raster=%6.2f ms  tris=%5llu  frags=%7llu  -> %s\n",
                    variant.suffix, variant.vertexShader->name(), variant.fragmentShader->name(),
                    stats.timings.frameMs, stats.timings.rasterMs,
                    static_cast<unsigned long long>(stats.counters.trianglesRasterized),
                    static_cast<unsigned long long>(stats.counters.fragmentsShaded), path.c_str());
    }

    context.workingSetBytes = device.workingSetBytes();
    std::printf("\n");
    debug::printStats(device.stats(), context);
    return 0;
}
