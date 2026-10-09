// NOVA GPU - Software GPU
// src/debug/stats.h - per-frame instrumentation produced by the pipeline.
#pragma once

#include <cstdint>
#include <string>

namespace nova::debug {

// Canonical renderer name reported by the debug overlay, benchmarks and logs.
inline const char* rendererName() { return "NOVA GPU \xE2\x80\x94 CPU SOFTWARE RENDERER"; }

// Timing of the individual pipeline stages for one rendered frame.
struct StageTimings {
    double cullMs = 0.0;     // object visibility tests (frustum culling)
    double vertexMs = 0.0;   // vertex processing (transform + lighting)
    double clipMs = 0.0;     // near-plane clipping
    double setupMs = 0.0;    // triangle setup (cull, viewport, edge equations)
    double rasterMs = 0.0;   // tile rasterization + fragment shading
    double frameMs = 0.0;    // whole frame: clear -> present
};

// Counters describing how much work the frame actually did.
struct RenderCounters {
    uint64_t drawCalls = 0;
    uint64_t objectsSubmitted = 0;        // draw calls examined this frame
    uint64_t objectsCulled = 0;           // draw calls rejected by the frustum test
    uint64_t trianglesSubmitted = 0;      // triangles reaching the command processor
    uint64_t verticesProcessed = 0;
    uint64_t trianglesClipped = 0;        // triangles split by clipping
    uint64_t trianglesCulled = 0;         // back-face / degenerate
    uint64_t trianglesFrustumCulled = 0;  // skipped with a culled object
    uint64_t trianglesRasterized = 0;     // **visible**: binned + rasterized
    uint64_t tileTrianglePairs = 0;       // (tile, triangle) pairs sent to the tiler
    uint64_t fragmentsTested = 0;         // pixels covered by a triangle
    uint64_t fragmentsShaded = 0;         // pixels that survived the depth test
    uint64_t pixelsPerFrame = 0;
};

// Rolling window of recent frame times, drawn as a graph by the debug overlay.
struct FrameTimeHistory {
    static constexpr int kCapacity = 120;

    float samples[kCapacity] = {};
    int count = 0;  // valid samples, <= kCapacity
    int head = 0;   // next write position

    void push(double ms) {
        samples[head] = static_cast<float>(ms);
        head = (head + 1) % kCapacity;
        if (count < kCapacity) ++count;
    }

    // 0 = oldest sample still in the window.
    float at(int index) const {
        const int start = (head - count + kCapacity) % kCapacity;
        return samples[(start + index) % kCapacity];
    }

    double average() const {
        if (count == 0) return 0.0;
        double sum = 0.0;
        for (int i = 0; i < count; ++i) sum += at(i);
        return sum / count;
    }
    float minimum() const {
        if (count == 0) return 0.0f;
        float m = at(0);
        for (int i = 1; i < count; ++i) m = at(i) < m ? at(i) : m;
        return m;
    }
    float maximum() const {
        if (count == 0) return 0.0f;
        float m = at(0);
        for (int i = 1; i < count; ++i) m = at(i) > m ? at(i) : m;
        return m;
    }
    void clear() {
        count = 0;
        head = 0;
    }
};

// Everything the debug mode displays.
struct RenderStats {
    StageTimings timings;
    RenderCounters counters;
    double fps = 0.0;
    int threadCount = 1;
    int width = 0;
    int height = 0;
    size_t framebufferBytes = 0;  // colour + depth buffer
    size_t workingSetBytes = 0;   // framebuffer + rasterizer bins/queues
    std::string renderer = rendererName();

    // Phase 6 debug mode: frame-time window and the shaders of the last frame.
    FrameTimeHistory frameTimes;
    std::string vertexShader = "none";
    std::string fragmentShader = "none";

    void resetCounters() { counters = RenderCounters{}; }

    // Triangles that produced pixels, i.e. the visible triangle count.
    uint64_t visibleTriangles() const { return counters.trianglesRasterized; }
};

}  // namespace nova::debug
