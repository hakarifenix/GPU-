// NOVA GPU - Software GPU
// src/core/config.h - renderer configuration.
#pragma once

#include <cstdint>

namespace nova::core {

struct RenderConfig {
    int width = 640;
    int height = 480;

    // Tiled rasterization.
    int tileSize = 32;          // pixels per tile edge (must be > 0)
    bool enableThreads = true;  // false forces single-threaded rasterization
    int threadCount = 0;        // 0 => std::thread::hardware_concurrency()

    // Rasterizer state.
    bool backfaceCulling = true;
    bool frontFaceCCW = true;   // front faces are counter-clockwise in NDC
    bool depthTest = true;
    bool depthWrite = true;
    bool clipNearPlane = true;  // near-plane clipping + w > 0 clipping
    bool cullDegenerate = true; // drop zero-area triangles

    bool clearColor = true;
    bool clearDepth = true;
};

}  // namespace nova::core
