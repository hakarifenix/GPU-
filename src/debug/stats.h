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
    double vertexMs = 0.0;   // vertex processing (transform + lighting)
    double clipMs = 0.0;     // near-plane clipping
    double setupMs = 0.0;    // triangle setup (cull, viewport, edge equations)
    double rasterMs = 0.0;   // tile rasterization + fragment shading
    double frameMs = 0.0;    // whole frame: clear -> present
};

// Counters describing how much work the frame actually did.
struct RenderCounters {
    uint64_t drawCalls = 0;
    uint64_t trianglesSubmitted = 0;  // triangles reaching the command processor
    uint64_t verticesProcessed = 0;
    uint64_t trianglesClipped = 0;    // triangles split by clipping
    uint64_t trianglesCulled = 0;     // back-face / degenerate
    uint64_t trianglesRasterized = 0; // triangles binned + rasterized
    uint64_t fragmentsTested = 0;     // pixels covered by a triangle
    uint64_t fragmentsShaded = 0;     // pixels that survived the depth test
    uint64_t pixelsPerFrame = 0;
};

// Everything the debug mode displays.
struct RenderStats {
    StageTimings timings;
    RenderCounters counters;
    double fps = 0.0;
    int threadCount = 1;
    int width = 0;
    int height = 0;
    size_t framebufferBytes = 0;
    std::string renderer = rendererName();

    void resetCounters() { counters = RenderCounters{}; }
};

}  // namespace nova::debug
