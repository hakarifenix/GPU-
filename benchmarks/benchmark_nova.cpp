// NOVA GPU - Software GPU
// benchmarks/benchmark_nova.cpp - Phase 1 benchmark harness.
//
// Measures the renderer on scenes of ~100 / ~1,000 / ~10,000 / ~50,000
// triangles and prints FPS, frame time and the per-stage breakdown produced by
// the device's internal timers (vertex / clip+setup / raster+fragment).
//
// Design notes:
//   * Every scene is the same lattice of cubes scaled to fill the same part of
//     the view, so the scenes differ in triangle count and overdraw, not in
//     screen coverage. Otherwise the numbers would not be comparable.
//   * Scene construction happens before timing starts; only the frame loop is
//     measured. A warm-up pass lets the allocator and caches settle.
//   * Timings come from wall-clock around beginFrame/draw/endFrame, so they
//     include everything the renderer does, not just the rasterizer.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "camera/camera.h"
#include "core/config.h"
#include "math/math_utils.h"
#include "debug/report.h"
#include "gpu/device.h"
#include "platform/console.h"
#include "scene/shapes.h"
#include "scene/transform.h"
#include "shader/color_shader.h"

using namespace nova;

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kTargetSceneTriangles[4] = {100, 1000, 10000, 50000};
constexpr double kTargetFramesPerScene = 2.0e7;  // frame budget per scene

struct Scene {
    scene::Mesh mesh;
    int triangles = 0;
    int cubes = 0;
    size_t meshBytes = 0;
};

// Builds a lattice of cubes containing at least `targetTriangles` triangles.
// The lattice always spans kLatticeSpan world units so every scene fills a
// similar portion of the viewport.
Scene buildScene(int targetTriangles) {
    constexpr float kLatticeSpan = 24.0f;
    const scene::Mesh unitCube = scene::makeCube(1.0f);
    const int cubes = (targetTriangles + 11) / 12;

    int side = 1;
    while (side * side * side < cubes) ++side;
    const float spacing = kLatticeSpan / static_cast<float>(side);
    const float cubeScale = std::min(1.0f, spacing * 0.7f);

    Scene scene;
    scene.cubes = cubes;
    scene.mesh.vertices.reserve(static_cast<size_t>(cubes) * unitCube.vertexCount());
    scene.mesh.indices.reserve(static_cast<size_t>(cubes) * unitCube.indexCount());

    const float offset = (static_cast<float>(side) - 1.0f) * 0.5f;
    int remaining = targetTriangles;

    for (int c = 0; c < cubes && remaining > 0; ++c) {
        const int ix = c % side;
        const int iy = (c / side) % side;
        const int iz = c / (side * side);

        const math::Vec3 center{(static_cast<float>(ix) - offset) * spacing,
                                (static_cast<float>(iy) - offset) * spacing,
                                (static_cast<float>(iz) - offset) * spacing};

        const uint32_t base = static_cast<uint32_t>(scene.mesh.vertices.size());
        for (size_t v = 0; v < unitCube.vertexCount(); ++v) {
            core::Vertex vertex = unitCube.vertices[v];
            vertex.position = vertex.position * cubeScale + center;
            scene.mesh.vertices.push_back(vertex);
        }

        const int trianglesHere = std::min(12, remaining);
        for (int t = 0; t < trianglesHere; ++t) {
            scene.mesh.addTriangle(base + unitCube.indices[t * 3 + 0],
                                   base + unitCube.indices[t * 3 + 1],
                                   base + unitCube.indices[t * 3 + 2]);
        }
        remaining -= trianglesHere;
    }

    scene.triangles = static_cast<int>(scene.mesh.triangleCount());
    scene.meshBytes = scene.mesh.memoryBytes();
    return scene;
}

struct Measurement {
    double frameMs = 0.0;
    double fps = 0.0;
    double vertexMs = 0.0;
    double setupMs = 0.0;
    double rasterMs = 0.0;
    uint64_t triangles = 0;
    uint64_t fragmentsTested = 0;
    uint64_t fragmentsShaded = 0;
    size_t workingSetBytes = 0;
};

Measurement runScene(const Scene& scene, int threadCount, int frames, int tileSize, int width,
                     int height) {
    core::RenderConfig config;
    config.width = width;
    config.height = height;
    config.tileSize = tileSize;
    config.enableThreads = threadCount > 1;
    config.threadCount = threadCount;
    config.backfaceCulling = true;
    config.depthTest = true;

    gpu::Device device(config);
    const shader::ColorFragmentShader shader;

    camera::Camera cam;
    cam.setPerspective(60.0f, static_cast<float>(width) / static_cast<float>(height), 0.5f, 200.0f);
    cam.setPosition({0.0f, 0.0f, 34.0f});
    cam.setTarget({0.0f, 0.0f, 0.0f});

    const int warmupFrames = 5;
    Measurement result;

    for (int frame = 0; frame < warmupFrames + frames; ++frame) {
        const float angle = math::radians(1.5f) * static_cast<float>(frame);
        const math::Mat4 model = scene::makeTransform({}, {angle * 0.4f, angle, 0.0f});

        const auto start = std::chrono::steady_clock::now();
        device.beginFrame(core::kDarkGray);
        device.draw(scene.mesh, model, cam.viewProjection(), shader);
        device.endFrame();
        const double frameMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();

        if (frame < warmupFrames) continue;  // discard warm-up

        result.frameMs += frameMs;
        result.vertexMs += device.stats().timings.vertexMs;
        result.setupMs += device.stats().timings.setupMs;
        result.rasterMs += device.stats().timings.rasterMs;
        result.triangles += device.stats().counters.trianglesSubmitted;
        result.fragmentsTested += device.stats().counters.fragmentsTested;
        result.fragmentsShaded += device.stats().counters.fragmentsShaded;
    }

    const double inv = 1.0 / static_cast<double>(frames);
    result.frameMs *= inv;
    result.vertexMs *= inv;
    result.setupMs *= inv;
    result.rasterMs *= inv;
    result.fps = result.frameMs > 0.0 ? 1000.0 / result.frameMs : 0.0;
    result.workingSetBytes = device.workingSetBytes();
    result.triangles = static_cast<uint64_t>(result.triangles / static_cast<uint64_t>(frames));
    result.fragmentsTested = static_cast<uint64_t>(result.fragmentsTested / static_cast<uint64_t>(frames));
    result.fragmentsShaded = static_cast<uint64_t>(result.fragmentsShaded / static_cast<uint64_t>(frames));

    return result;
}

}  // namespace

int main(int argc, char** argv) {
    platform::enableAnsiColors();

    int width = 640;
    int height = 480;
    int tileSize = 32;
    int threads = static_cast<int>(std::thread::hardware_concurrency());
    if (threads <= 0) threads = 1;
    bool threadSweep = true;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const bool hasNext = (i + 1) < argc;
        if (std::strcmp(arg, "--width") == 0 && hasNext) {
            width = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--height") == 0 && hasNext) {
            height = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--tile") == 0 && hasNext) {
            tileSize = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--threads") == 0 && hasNext) {
            threads = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--no-thread-sweep") == 0) {
            threadSweep = false;
        } else if (std::strcmp(arg, "--help") == 0) {
            std::printf(
                "NOVA GPU benchmark\n"
                "  --width N / --height N   framebuffer size (default 640x480)\n"
                "  --tile N                 tile size in pixels (default 32)\n"
                "  --threads N              worker threads (default: hardware concurrency)\n"
                "  --no-thread-sweep        skip the 1/2/4/8-thread scaling comparison\n");
            return 0;
        } else {
            std::fprintf(stderr, "unknown argument: %s (try --help)\n", arg);
            return 2;
        }
    }

    std::printf("%s\n", debug::rendererName());
    std::printf("resolution %dx%d, tile %d px, threads %d (of %u hardware)\n\n", width, height,
                tileSize, threads, std::thread::hardware_concurrency());

    std::printf("=== triangle count scaling (threads = %d) ===\n", threads);
    std::printf("%9s %9s %8s %9s %8s %8s %8s %10s %10s %9s\n", "target", "tris", "frames",
                "frame ms", "FPS", "vertex", "setup", "frag tested", "frag shaded", "mem MiB");

    for (const int target : kTargetSceneTriangles) {
        const Scene scene = buildScene(target);
        const int frames = static_cast<int>(
            std::max(8.0, std::min(200.0, kTargetFramesPerScene / static_cast<double>(scene.triangles))));
        const Measurement m = runScene(scene, threads, frames, tileSize, width, height);

        std::printf("%9d %9d %8d %9.2f %8.1f %8.3f %8.3f %10s %10s %9.2f\n", target,
                    scene.triangles, frames, m.frameMs, m.fps, m.vertexMs, m.setupMs,
                    platform::formatThousands(m.fragmentsTested).c_str(),
                    platform::formatThousands(m.fragmentsShaded).c_str(),
                    static_cast<double>(m.workingSetBytes + scene.meshBytes) / (1024.0 * 1024.0));
    }

    if (threadSweep) {
        const Scene scene = buildScene(kTargetSceneTriangles[3]);
        const int frames = 40;

        std::printf("\n=== thread scaling (%d triangles) ===\n", scene.triangles);
        std::printf("%9s %10s %8s %9s %9s %9s %10s\n", "threads", "workers", "frames", "frame ms",
                    "FPS", "speedup", "raster ms");

        double baseline = 0.0;
        for (const int count : {1, 2, 4, 8}) {
            if (count > threads && count != 1) continue;
            const Measurement m = runScene(scene, count, frames, tileSize, width, height);
            if (baseline == 0.0) baseline = m.frameMs;
            std::printf("%9d %10d %8d %9.2f %9.1f %8.2fx %10.3f\n", count,
                        count > 1 ? count - 1 : 0, frames, m.frameMs, m.fps,
                        baseline / m.frameMs, m.rasterMs);
        }
    }

    std::printf("\nnote: 'vertex' is per-vertex transform, 'setup' is clip+triangle setup+binning,\n"
                "      'raster' is tile rasterization + fragment shading for the whole frame.\n");
    std::printf("note: tile rasterization is parallelized over tiles. A speedup needs more than\n"
                "      one usable CPU core; on a single-core or CPU-quota-limited VM the scaling\n"
                "      numbers will be flat and are reported as measured.\n");
    return 0;
}
