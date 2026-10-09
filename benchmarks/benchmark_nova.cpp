// NOVA GPU - Software GPU
// benchmarks/benchmark_nova.cpp - Phase 5 benchmark / profiler.
//
// Three modes:
//   default          triangle-count scaling table (100 / 1,000 / 10,000 / 50,000)
//   --profile        one scene with a per-stage breakdown and work counters
//   --compare        the same scene with each optimisation turned on/off, so a
//                    speedup claim can be checked instead of assumed
//   --culling-scene  many small objects spread over a wide area, which is what
//                    frustum culling is actually for
//
// Design notes:
//   * Scenes are built before timing; only the frame loop is measured, and a few
//     warm-up frames are discarded so the allocator and caches have settled.
//   * Frames run the full Device API (beginFrame / draw / endFrame), so the
//     numbers include everything the renderer does, not just the rasterizer.
//   * Every measurement is repeated; the reported number is the best of the
//     repeats, because the interesting question is "how fast can this run", and
//     a noisy VM only ever adds time.
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
#include "debug/report.h"
#include "gpu/device.h"
#include "math/math_utils.h"
#include "platform/console.h"
#include "scene/shapes.h"
#include "scene/transform.h"
#include "shader/color_shader.h"

using namespace nova;

namespace {

constexpr int kTargetSceneTriangles[4] = {100, 1000, 10000, 50000};
constexpr double kTargetFramesPerScene = 2.0e7;

struct DrawItem {
    scene::Mesh mesh;
    math::Mat4 model = math::Mat4::identity();
};

struct Scene {
    std::string name;
    std::vector<DrawItem> items;
    int triangles = 0;
    int objects = 0;
    size_t meshBytes = 0;
    bool animate = true;
    bool wideLayout = false;  // spread objects out so most leave the frustum
};

// Lattice of cubes filling a fixed world span: the standard scaling scene, where
// every scene has similar screen coverage and only the triangle count changes.
Scene buildLatticeScene(int targetTriangles) {
    constexpr float kLatticeSpan = 24.0f;
    const scene::Mesh unitCube = scene::makeCube(1.0f);
    const int cubes = (targetTriangles + 11) / 12;

    int side = 1;
    while (side * side * side < cubes) ++side;
    const float spacing = kLatticeSpan / static_cast<float>(side);
    const float cubeScale = std::min(1.0f, spacing * 0.7f);

    scene::Mesh mesh;
    mesh.vertices.reserve(static_cast<size_t>(cubes) * unitCube.vertexCount());
    mesh.indices.reserve(static_cast<size_t>(cubes) * unitCube.indexCount());

    const float offset = (static_cast<float>(side) - 1.0f) * 0.5f;
    int remaining = targetTriangles;

    for (int c = 0; c < cubes && remaining > 0; ++c) {
        const int ix = c % side;
        const int iy = (c / side) % side;
        const int iz = c / (side * side);
        const math::Vec3 center{(static_cast<float>(ix) - offset) * spacing,
                                (static_cast<float>(iy) - offset) * spacing,
                                (static_cast<float>(iz) - offset) * spacing};

        const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        for (size_t v = 0; v < unitCube.vertexCount(); ++v) {
            core::Vertex vertex = unitCube.vertices[v];
            vertex.position = vertex.position * cubeScale + center;
            mesh.vertices.push_back(vertex);
        }
        const int trianglesHere = std::min(12, remaining);
        for (int t = 0; t < trianglesHere; ++t) {
            mesh.addTriangle(base + unitCube.indices[t * 3 + 0], base + unitCube.indices[t * 3 + 1],
                             base + unitCube.indices[t * 3 + 2]);
        }
        remaining -= trianglesHere;
    }

    Scene scene;
    scene.name = "lattice";
    scene.items.push_back(DrawItem{std::move(mesh), math::Mat4::identity()});
    scene.triangles = static_cast<int>(scene.items[0].mesh.triangleCount());
    scene.objects = 1;
    scene.meshBytes = scene.items[0].mesh.memoryBytes();
    return scene;
}

// Many separate objects spread over a wide area. The camera sees only the middle
// of the grid, so most draw calls can be rejected before any vertex is touched.
Scene buildCullingScene(int side, float span) {
    Scene scene;
    scene.name = "many-objects";
    scene.animate = false;
    scene.wideLayout = true;

    const scene::Mesh unitCube = scene::makeCube(1.4f);
    const float spacing = span / static_cast<float>(side);
    const float offset = (static_cast<float>(side) - 1.0f) * 0.5f;

    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            DrawItem item;
            item.mesh = unitCube;
            item.model = scene::makeTransform(
                {(static_cast<float>(x) - offset) * spacing,
                 (static_cast<float>(y) - offset) * spacing, 0.0f},
                {0.0f, 0.0f, 0.0f});
            scene.items.push_back(std::move(item));
        }
    }

    scene.triangles = static_cast<int>(unitCube.triangleCount()) * static_cast<int>(scene.items.size());
    scene.objects = static_cast<int>(scene.items.size());
    scene.meshBytes = unitCube.memoryBytes() * scene.items.size();
    return scene;
}

struct RunOptions {
    int width = 640;
    int height = 480;
    int tileSize = 32;
    int threads = 1;
    int frames = 60;
    int repeats = 3;
    bool frustumCulling = true;
    bool enableSimd = true;
};

struct Measurement {
    double frameMs = 0.0;
    double fps = 0.0;
    double cullMs = 0.0;
    double vertexMs = 0.0;
    double setupMs = 0.0;
    double rasterMs = 0.0;
    uint64_t triangles = 0;
    uint64_t visibleTriangles = 0;
    uint64_t objects = 0;
    uint64_t objectsCulled = 0;
    uint64_t trianglesFrustumCulled = 0;
    uint64_t fragmentsTested = 0;
    uint64_t fragmentsShaded = 0;
    uint64_t tileTrianglePairs = 0;
    size_t workingSetBytes = 0;
    size_t meshBytes = 0;
};

double measureOnce(const Scene& scene, const RunOptions& options, Measurement& out) {
    core::RenderConfig config;
    config.width = options.width;
    config.height = options.height;
    config.tileSize = options.tileSize;
    config.enableThreads = options.threads > 1;
    config.threadCount = options.threads;
    config.frustumCulling = options.frustumCulling;
    config.enableSimd = options.enableSimd;

    gpu::Device device(config);
    const shader::ColorFragmentShader shader;

    camera::Camera cam;
    cam.setPerspective(60.0f, static_cast<float>(options.width) / static_cast<float>(options.height),
                       0.5f, 400.0f);
    cam.setPosition({0.0f, 0.0f, scene.wideLayout ? 26.0f : 34.0f});
    cam.setTarget({0.0f, 0.0f, 0.0f});

    const int warmupFrames = 3;
    Measurement total;
    for (int frame = 0; frame < warmupFrames + options.frames; ++frame) {
        double frameMs = 0.0;
        const auto start = std::chrono::steady_clock::now();
        device.beginFrame(core::kDarkGray);
        for (size_t i = 0; i < scene.items.size(); ++i) {
            const DrawItem& item = scene.items[i];
            const math::Mat4 model =
                scene.animate
                    ? scene::makeTransform({}, {math::radians(1.5f) * frame * 0.4f,
                                                math::radians(1.5f) * frame, 0.0f})
                    : item.model;
            device.draw(item.mesh, model, cam.viewProjection(), shader);
        }
        device.endFrame();
        frameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                      .count();

        if (frame < warmupFrames) continue;

        const debug::RenderStats& stats = device.stats();
        total.frameMs += frameMs;
        total.cullMs += stats.timings.cullMs;
        total.vertexMs += stats.timings.vertexMs;
        total.setupMs += stats.timings.setupMs;
        total.rasterMs += stats.timings.rasterMs;
        total.triangles += stats.counters.trianglesSubmitted;
        total.visibleTriangles += stats.counters.trianglesRasterized;
        total.objects += stats.counters.objectsSubmitted;
        total.objectsCulled += stats.counters.objectsCulled;
        total.trianglesFrustumCulled += stats.counters.trianglesFrustumCulled;
        total.fragmentsTested += stats.counters.fragmentsTested;
        total.fragmentsShaded += stats.counters.fragmentsShaded;
        total.tileTrianglePairs += stats.counters.tileTrianglePairs;
    }

    const double inv = 1.0 / static_cast<double>(options.frames);
    total.frameMs *= inv;
    total.cullMs *= inv;
    total.vertexMs *= inv;
    total.setupMs *= inv;
    total.rasterMs *= inv;
    total.fps = total.frameMs > 0.0 ? 1000.0 / total.frameMs : 0.0;
    total.triangles /= options.frames;
    total.visibleTriangles /= options.frames;
    total.objects /= options.frames;
    total.objectsCulled /= options.frames;
    total.trianglesFrustumCulled /= options.frames;
    total.fragmentsTested /= options.frames;
    total.fragmentsShaded /= options.frames;
    total.tileTrianglePairs /= options.frames;
    total.workingSetBytes = device.workingSetBytes();
    total.meshBytes = scene.meshBytes;

    out = total;
    return total.frameMs;
}

// Runs the scene several times and keeps the fastest (and its matching stats).
Measurement runScene(const Scene& scene, const RunOptions& options) {
    Measurement best;
    double bestMs = 0.0;
    for (int repeat = 0; repeat < options.repeats; ++repeat) {
        Measurement current;
        const double ms = measureOnce(scene, options, current);
        if (bestMs == 0.0 || ms < bestMs) {
            bestMs = ms;
            best = current;
        }
    }
    return best;
}

// Returns by value: a shared buffer would make every argument of one printf
// point at the same (last written) string.
std::string formatThousands(uint64_t value) { return nova::platform::formatThousands(value); }

void printStageBreakdown(const Measurement& m) {
    const double frame = m.frameMs > 0.0 ? m.frameMs : 1.0;
    const double accounted = m.cullMs + m.vertexMs + m.setupMs + m.rasterMs;
    const double other = m.frameMs - accounted;

    std::printf("  %-16s %9s %9s\n", "stage", "ms/frame", "share");
    std::printf("  %-16s %9.3f %8.1f%%\n", "cull", m.cullMs, m.cullMs / frame * 100.0);
    std::printf("  %-16s %9.3f %8.1f%%\n", "vertex", m.vertexMs, m.vertexMs / frame * 100.0);
    std::printf("  %-16s %9.3f %8.1f%%\n", "clip+setup+bin", m.setupMs, m.setupMs / frame * 100.0);
    std::printf("  %-16s %9.3f %8.1f%%\n", "raster+frag", m.rasterMs, m.rasterMs / frame * 100.0);
    std::printf("  %-16s %9.3f %8.1f%%\n", "clear+overhead", other, other / frame * 100.0);
    std::printf("  %-16s %9.3f\n", "frame", m.frameMs);

    std::printf("  work: %s tris submitted, %s visible, %s culled by frustum (%.1f%%)\n",
                formatThousands(m.triangles).c_str(), formatThousands(m.visibleTriangles).c_str(),
                formatThousands(m.trianglesFrustumCulled).c_str(),
                m.triangles > 0 ? 100.0 * static_cast<double>(m.trianglesFrustumCulled) /
                                      static_cast<double>(m.triangles)
                                : 0.0);
    std::printf("  work: %s objects, %s culled; %s tile-triangle pairs\n",
                formatThousands(m.objects).c_str(), formatThousands(m.objectsCulled).c_str(),
                formatThousands(m.tileTrianglePairs).c_str());
    const double tested = static_cast<double>(m.fragmentsTested);
    const double perVisibleTri = m.visibleTriangles > 0
                                     ? tested / static_cast<double>(m.visibleTriangles)
                                     : 0.0;
    std::printf("  work: %s fragments tested, %s shaded (%.1f frags/visible tri, %.0f%% pass depth)\n",
                formatThousands(m.fragmentsTested).c_str(), formatThousands(m.fragmentsShaded).c_str(),
                perVisibleTri,
                tested > 0 ? 100.0 * static_cast<double>(m.fragmentsShaded) / tested : 0.0);
    std::printf("  memory: geometry %.2f MiB, framebuffer %.2f MiB, working set %.2f MiB\n",
                static_cast<double>(m.meshBytes) / (1024.0 * 1024.0),
                static_cast<double>(m.workingSetBytes) / (1024.0 * 1024.0),
                static_cast<double>(m.workingSetBytes + m.meshBytes) / (1024.0 * 1024.0));
}

}  // namespace

int main(int argc, char** argv) {
    platform::enableAnsiColors();

    RunOptions options;
    int threads = static_cast<int>(std::thread::hardware_concurrency());
    if (threads <= 0) threads = 1;
    bool threadSweep = true;
    bool profile = false;
    bool compare = false;
    bool cullingScene = false;
    int repeats = 3;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const bool hasNext = (i + 1) < argc;
        if (std::strcmp(arg, "--width") == 0 && hasNext) {
            options.width = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--height") == 0 && hasNext) {
            options.height = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--tile") == 0 && hasNext) {
            options.tileSize = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--threads") == 0 && hasNext) {
            threads = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--frames") == 0 && hasNext) {
            options.frames = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--repeats") == 0 && hasNext) {
            repeats = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--no-thread-sweep") == 0) {
            threadSweep = false;
        } else if (std::strcmp(arg, "--profile") == 0) {
            profile = true;
        } else if (std::strcmp(arg, "--compare") == 0) {
            compare = true;
        } else if (std::strcmp(arg, "--culling-scene") == 0) {
            cullingScene = true;
        } else if (std::strcmp(arg, "--help") == 0) {
            std::printf(
                "NOVA GPU benchmark / profiler (Phase 5)\n"
                "  --width N / --height N   framebuffer size (default 640x480)\n"
                "  --tile N                 tile size in pixels (default 32)\n"
                "  --threads N              worker threads (default: hardware concurrency)\n"
                "  --frames N               frames per measurement (default: scene dependent)\n"
                "  --repeats N              repeats per measurement, best is reported (default 3)\n"
                "  --profile                per-stage breakdown of one scene\n"
                "  --compare                optimisations on/off side by side\n"
                "  --culling-scene          many small objects spread over a wide area\n"
                "  --no-thread-sweep        skip the 1/2/4/8-thread scaling comparison\n");
            return 0;
        } else {
            std::fprintf(stderr, "unknown argument: %s (try --help)\n", arg);
            return 2;
        }
    }

    options.threads = threads;
    options.repeats = repeats < 1 ? 1 : repeats;

    std::printf("%s\n", debug::rendererName());
    std::printf("resolution %dx%d, tile %d px, threads %d (of %u hardware)\n\n", options.width,
                options.height, options.tileSize, threads, std::thread::hardware_concurrency());

    if (cullingScene) {
        const Scene scene = buildCullingScene(24, 220.0f);
        std::printf("=== frustum culling: %d objects, %d triangles, spread over 220 units ===\n",
                    scene.objects, scene.triangles);
        std::printf("%14s %10s %10s %10s %9s %9s %10s %10s\n", "culling", "frame ms", "FPS",
                    "cull ms", "vertex", "setup", "raster", "objs culled");

        RunOptions cullingOff = options;
        cullingOff.frames = options.frames > 0 ? 20 : 20;
        RunOptions cullingOn = cullingOff;
        cullingOn.frustumCulling = true;
        cullingOff.frustumCulling = false;

        const Measurement off = runScene(scene, cullingOff);
        const Measurement on = runScene(scene, cullingOn);

        std::printf("%14s %10.2f %10.1f %10.3f %9.3f %9.3f %10.3f %10s\n", "off", off.frameMs,
                    off.fps, off.cullMs, off.vertexMs, off.setupMs, off.rasterMs,
                    formatThousands(off.objectsCulled).c_str());
        std::printf("%14s %10.2f %10.1f %10.3f %9.3f %9.3f %10.3f %10s\n", "on", on.frameMs, on.fps,
                    on.cullMs, on.vertexMs, on.setupMs, on.rasterMs,
                    formatThousands(on.objectsCulled).c_str());
        std::printf("speedup: %.2fx frame time, %.1f%% of objects culled (%s visible triangles)\n",
                    off.frameMs / (on.frameMs > 0.0 ? on.frameMs : 1.0),
                    off.objects > 0 ? 100.0 * static_cast<double>(on.objectsCulled) /
                                          static_cast<double>(off.objects)
                                    : 0.0,
                    formatThousands(on.visibleTriangles).c_str());

        if (profile) {
            std::printf("\n-- culling on --\n");
            printStageBreakdown(on);
            std::printf("\n-- culling off --\n");
            printStageBreakdown(off);
        }
        return 0;
    }

    std::printf("=== triangle count scaling (threads = %d) ===\n", threads);
    std::printf("%9s %9s %8s %9s %8s %8s %8s %10s %10s %10s %9s\n", "target", "tris", "frames",
                "frame ms", "FPS", "vertex", "setup", "visible", "frag tested", "frag shaded",
                "mem MiB");

    Measurement last;
    Scene lastScene;
    for (const int target : kTargetSceneTriangles) {
        const Scene scene = buildLatticeScene(target);
        RunOptions sceneOptions = options;
        sceneOptions.frames = static_cast<int>(std::max(
            8.0, std::min(120.0, kTargetFramesPerScene / static_cast<double>(scene.triangles))));
        const Measurement m = runScene(scene, sceneOptions);

        std::printf("%9d %9d %8d %9.2f %8.1f %8.3f %8.3f %10s %10s %10s %9.2f\n", target,
                    scene.triangles, sceneOptions.frames, m.frameMs, m.fps, m.vertexMs, m.setupMs,
                    formatThousands(m.visibleTriangles).c_str(), formatThousands(m.fragmentsTested).c_str(),
                    formatThousands(m.fragmentsShaded).c_str(),
                    static_cast<double>(m.workingSetBytes + scene.meshBytes) / (1024.0 * 1024.0));
        last = m;
        lastScene = scene;
    }

    if (profile) {
        std::printf("\n=== profile: %d triangles (%dx%d, tile %d, threads %d) ===\n",
                    lastScene.triangles, options.width, options.height, options.tileSize, threads);
        printStageBreakdown(last);
    }

    if (compare) {
        const Scene scene = buildLatticeScene(50000);
        RunOptions base = options;
        base.frames = 30;

        struct Variant {
            const char* label;
            bool culling;
            bool simd;
        };
        constexpr int kVariantCount = 4;
        const Variant variants[kVariantCount] = {
            {"all off", false, false},
            {"frustum culling", true, false},
            {"simd", false, true},
            {"both on", true, true},
        };

        std::printf("\n=== optimisation A/B: %d triangles ===\n", scene.triangles);
        std::printf("%18s %10s %9s %9s %9s %10s %11s %11s\n", "settings", "frame ms", "cull ms",
                    "vertex", "setup", "raster", "visible", "objs culled");

        // Each variant is measured once per round, cycling through all of them,
        // so every variant sees the same stretch of machine time. Measuring them
        // one after another instead biases whichever runs last: this VM drifts by
        // tens of percent over a few seconds and would make "both on" look like it
        // beat the sum of its parts.
        constexpr int kCompareRounds = 4;
        Measurement best[kVariantCount];
        bool haveBest[kVariantCount] = {};
        for (int round = 0; round < kCompareRounds; ++round) {
            for (int i = 0; i < kVariantCount; ++i) {
                RunOptions variantOptions = base;
                variantOptions.repeats = 1;  // the interleaved rounds are the repeats
                variantOptions.frustumCulling = variants[i].culling;
                variantOptions.enableSimd = variants[i].simd;
                const Measurement m = runScene(scene, variantOptions);
                if (!haveBest[i] || m.frameMs < best[i].frameMs) {
                    best[i] = m;
                    haveBest[i] = true;
                }
            }
        }

        const double baseline = best[0].frameMs;
        for (int i = 0; i < kVariantCount; ++i) {
            const Measurement& m = best[i];
            std::printf("%18s %10.2f %9.3f %9.3f %9.3f %10.3f %11s %11s\n", variants[i].label, m.frameMs,
                        m.cullMs, m.vertexMs, m.setupMs, m.rasterMs,
                        formatThousands(m.visibleTriangles).c_str(), formatThousands(m.objectsCulled).c_str());
        }
        std::printf("baseline frame time %.2f ms (best of %d interleaved rounds per variant)\n",
                    baseline, kCompareRounds);
        std::printf("note: the lattice scene fills the view, so frustum culling has almost nothing\n"
                    "      to reject there - use --culling-scene to measure it.\n");
    }

    if (threadSweep) {
        const Scene scene = buildLatticeScene(kTargetSceneTriangles[3]);
        std::printf("\n=== thread scaling (%d triangles) ===\n", scene.triangles);
        std::printf("%9s %10s %8s %9s %9s %9s %10s\n", "threads", "workers", "frames", "frame ms",
                    "FPS", "speedup", "raster ms");

        double baseline = 0.0;
        for (const int count : {1, 2, 4, 8}) {
            if (count > threads && count != 1) continue;
            RunOptions sceneOptions = options;
            sceneOptions.threads = count;
            sceneOptions.frames = 30;
            const Measurement m = runScene(scene, sceneOptions);
            if (baseline == 0.0) baseline = m.frameMs;
            std::printf("%9d %10d %8d %9.2f %9.1f %8.2fx %10.3f\n", count, count > 1 ? count - 1 : 0,
                        sceneOptions.frames, m.frameMs, m.fps, baseline / m.frameMs, m.rasterMs);
        }
    }

    std::printf("\nnote: 'vertex' is per-vertex transform, 'setup' is clip+triangle setup+binning,\n"
                "      'raster' is tile rasterization + fragment shading for the whole frame.\n"
                "note: each measurement runs several times and reports the fastest run, so noise\n"
                "      from the VM only ever adds time. 'visible' counts triangles that produced\n"
                "      pixels (binned and rasterized).\n"
                "note: tile rasterization is parallelized over tiles. A speedup needs more than\n"
                "      one usable CPU core; on a single-core or CPU-quota-limited VM the scaling\n"
                "      numbers will be flat and are reported as measured.\n");
    return 0;
}
