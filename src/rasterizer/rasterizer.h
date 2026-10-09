// NOVA GPU - Software GPU
// src/rasterizer/rasterizer.h - stage 5/6: tiled software rasterizer.
//
// Tiling model (the reason the rasterizer is tiled rather than scanline-based):
//   1. submit() bins every triangle once into the tiles its bounding box covers.
//   2. flush() rasterizes one tile per parallel job.
// That gives the parallel loop a large number of independent, similarly sized
// jobs, so all cores stay busy on both tiny and huge triangles, and each tile
// touches a small, cache-resident region of the color/depth buffers.
//
// Interpolation:
//   * color / uv / world / normal are interpolated perspective-correctly
//     (attribute/w is affine in screen space, then divided by the interpolated 1/w)
//   * depth is interpolated linearly in screen space, which is exact for a
//     projective transform
//
// Fragment order within a triangle is deterministic (row-major, left to right),
// which keeps output stable across thread counts.
#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "core/config.h"
#include "core/framebuffer.h"
#include "core/thread_pool.h"
#include "debug/stats.h"
#include "pipeline/triangle_setup.h"
#include "shader/fragment_shader.h"
#include "shader/uniforms.h"

namespace nova::rasterizer {

class Rasterizer {
public:
    Rasterizer(const core::RenderConfig& config, core::Framebuffer& framebuffer,
               core::ThreadPool* pool);

    // Resets per-frame state (triangle list, tile bins, counters).
    void beginFrame();

    // Registers a uniforms block for this frame and returns its index. One
    // block is uploaded per draw call, so queuing a triangle only stores a small
    // integer instead of a copy of the whole constant block.
    uint32_t addUniformBlock(const shader::ShaderUniforms& uniforms);

    // Queues a viewport-space triangle together with the shader that shades it
    // and the index returned by addUniformBlock(). Thread-unsafe: the command
    // processor submits from a single thread.
    void submit(const pipeline::ScreenTriangle& triangle, const shader::FragmentShader& shader,
                uint32_t uniformBlock);

    // Rasterizes everything submitted since beginFrame(), shading each fragment
    // with the shader the triangle was submitted with.
    void flush();

    const debug::RenderCounters& counters() const { return counters_; }

    int tileSize() const { return tileSize_; }
    int tilesX() const { return tilesX_; }
    int tilesY() const { return tilesY_; }
    size_t tileCount() const { return static_cast<size_t>(tilesX_) * static_cast<size_t>(tilesY_); }

    // Number of (tile, triangle) pairs processed by the last flush().
    uint64_t tileTrianglePairs() const { return tileTrianglePairs_; }

    // Bytes held by the per-frame working set (queued triangles + tile bins).
    // Reported by the benchmark so memory use stays visible.
    size_t workingSetBytes() const;

private:
    // A queued triangle plus the shader state it was submitted with, so one
    // flush() can tile triangles from several draw calls in a single pass.
    struct WorkItem {
        pipeline::ScreenTriangle triangle;
        const shader::FragmentShader* shader = nullptr;
        uint32_t uniformBlock = 0;
    };

    void rasterizeTile(uint64_t tileIndex, uint64_t& fragmentsTested, uint64_t& fragmentsShaded);

    const core::RenderConfig& config_;
    core::Framebuffer& framebuffer_;
    core::ThreadPool* pool_;

    int width_ = 0;
    int height_ = 0;
    int tileSize_ = 32;
    int tilesX_ = 0;
    int tilesY_ = 0;

    std::vector<WorkItem> triangles_;
    std::vector<std::vector<uint32_t>> bins_;
    std::vector<shader::ShaderUniforms> uniformBlocks_;
    uint64_t tileTrianglePairs_ = 0;

    // Fragment counters are accumulated per tile and then merged, so workers
    // only touch the atomics once per tile instead of once per pixel.
    std::atomic<uint64_t> fragmentsTested_{0};
    std::atomic<uint64_t> fragmentsShaded_{0};
    debug::RenderCounters counters_;
};

}  // namespace nova::rasterizer
