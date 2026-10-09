#include "rasterizer/rasterizer.h"

#include <algorithm>

namespace nova::rasterizer {

Rasterizer::Rasterizer(const core::RenderConfig& config, core::Framebuffer& framebuffer,
                       core::ThreadPool* pool)
    : config_(config), framebuffer_(framebuffer), pool_(pool) {
    width_ = framebuffer_.width();
    height_ = framebuffer_.height();
    tileSize_ = config_.tileSize > 0 ? config_.tileSize : 32;
    tilesX_ = (width_ + tileSize_ - 1) / tileSize_;
    tilesY_ = (height_ + tileSize_ - 1) / tileSize_;
    bins_.resize(tileCount());
}

void Rasterizer::beginFrame() {
    triangles_.clear();
    uniformBlocks_.clear();
    for (std::vector<uint32_t>& bin : bins_) bin.clear();
    counters_ = debug::RenderCounters{};
    counters_.pixelsPerFrame =
        static_cast<uint64_t>(framebuffer_.pixelCount());
    fragmentsTested_.store(0, std::memory_order_relaxed);
    fragmentsShaded_.store(0, std::memory_order_relaxed);
    tileTrianglePairs_ = 0;
}

uint32_t Rasterizer::addUniformBlock(const shader::ShaderUniforms& uniforms) {
    uniformBlocks_.push_back(uniforms);
    return static_cast<uint32_t>(uniformBlocks_.size() - 1);
}

void Rasterizer::submit(const pipeline::ScreenTriangle& triangle,
                        const shader::FragmentShader& shader, uint32_t uniformBlock) {
    if (!triangle.hasCoverage()) return;

    const uint32_t index = static_cast<uint32_t>(triangles_.size());
    triangles_.push_back(WorkItem{triangle, &shader, uniformBlock});
    ++counters_.trianglesRasterized;

    const int tx0 = triangle.minX / tileSize_;
    const int tx1 = triangle.maxX / tileSize_;
    const int ty0 = triangle.minY / tileSize_;
    const int ty1 = triangle.maxY / tileSize_;

    for (int ty = ty0; ty <= ty1; ++ty) {
        const size_t rowBase = static_cast<size_t>(ty) * static_cast<size_t>(tilesX_);
        for (int tx = tx0; tx <= tx1; ++tx) {
            bins_[rowBase + static_cast<size_t>(tx)].push_back(index);
            ++tileTrianglePairs_;
        }
    }
}

void Rasterizer::rasterizeTile(uint64_t tileIndex, uint64_t& fragmentsTested,
                               uint64_t& fragmentsShaded) {
    const std::vector<uint32_t>& bin = bins_[tileIndex];
    if (bin.empty()) return;

    const int tx = static_cast<int>(tileIndex % static_cast<uint64_t>(tilesX_));
    const int ty = static_cast<int>(tileIndex / static_cast<uint64_t>(tilesX_));
    const int tileX0 = tx * tileSize_;
    const int tileY0 = ty * tileSize_;
    const int tileX1 = std::min(tileX0 + tileSize_, width_);
    const int tileY1 = std::min(tileY0 + tileSize_, height_);

    core::Color* const colorBase = framebuffer_.colorData();
    float* const depthBase = framebuffer_.depthData();
    const bool depthTest = config_.depthTest;
    const bool depthWrite = config_.depthWrite;

    for (const uint32_t index : bin) {
        const WorkItem& item = triangles_[index];
        const pipeline::ScreenTriangle& tri = item.triangle;
        if (item.shader == nullptr) continue;
        if (item.uniformBlock >= uniformBlocks_.size()) continue;
        const shader::FragmentShader& shader = *item.shader;
        const shader::ShaderUniforms& uniforms = uniformBlocks_[item.uniformBlock];

        const int x0 = std::max(tri.minX, tileX0);
        const int x1 = std::min(tri.maxX, tileX1 - 1);
        const int y0 = std::max(tri.minY, tileY0);
        const int y1 = std::min(tri.maxY, tileY1 - 1);
        if (x0 > x1 || y0 > y1) continue;

        // Edge weights at the first pixel center of this tile-triangle overlap,
        // then stepped across the row by the (constant) gradient.
        const float startX = static_cast<float>(x0) + 0.5f;
        const float startY = static_cast<float>(y0) + 0.5f;
        float w0Row = tri.weightA[0] * startX + tri.weightB[0] * startY + tri.weightC[0];
        float w1Row = tri.weightA[1] * startX + tri.weightB[1] * startY + tri.weightC[1];
        float w2Row = tri.weightA[2] * startX + tri.weightB[2] * startY + tri.weightC[2];

        const bool tl0 = tri.topLeft[0];
        const bool tl1 = tri.topLeft[1];
        const bool tl2 = tri.topLeft[2];

        for (int y = y0; y <= y1; ++y) {
            core::Color* const colorRow = colorBase + static_cast<size_t>(y) * width_;
            float* const depthRow = depthBase + static_cast<size_t>(y) * width_;

            float w0 = w0Row;
            float w1 = w1Row;
            float w2 = w2Row;

            for (int x = x0; x <= x1; ++x) {
                // w0..w2 are this pixel's barycentric weights. Everything below
                // (depth test, interpolation, shading) must use them *before*
                // the step to the next pixel center.
                const bool inside = (w0 > 0.0f || (w0 == 0.0f && tl0)) &&
                                    (w1 > 0.0f || (w1 == 0.0f && tl1)) &&
                                    (w2 > 0.0f || (w2 == 0.0f && tl2));
                if (inside) {
                    ++fragmentsTested;

                    const float depth = w0 * tri.depth[0] + w1 * tri.depth[1] + w2 * tri.depth[2];
                    float& storedDepth = depthRow[x];
                    if (!depthTest || depth < storedDepth) {
                        if (depthWrite) storedDepth = depth;
                        ++fragmentsShaded;

                        // Perspective-correct attribute interpolation: the stored
                        // values are attribute/w, so dividing by the interpolated
                        // 1/w recovers the attribute at this pixel.
                        const float invW = w0 * tri.invW[0] + w1 * tri.invW[1] + w2 * tri.invW[2];
                        const float attrScale = invW != 0.0f ? 1.0f / invW : 0.0f;

                        shader::FragmentInput input;
                        input.screenPos = {static_cast<float>(x) + 0.5f,
                                           static_cast<float>(y) + 0.5f};
                        input.depth = depth;
                        input.varying = (tri.varyingOverW[0] * w0 + tri.varyingOverW[1] * w1 +
                                         tri.varyingOverW[2] * w2) *
                                        attrScale;

                        colorRow[x] = core::colorFromVec4(shader.shade(input, uniforms));
                    }
                }

                w0 += tri.weightA[0];
                w1 += tri.weightA[1];
                w2 += tri.weightA[2];
            }

            w0Row += tri.weightB[0];
            w1Row += tri.weightB[1];
            w2Row += tri.weightB[2];
        }
    }
}

size_t Rasterizer::workingSetBytes() const {
    size_t bytes = triangles_.capacity() * sizeof(WorkItem) +
                   uniformBlocks_.capacity() * sizeof(shader::ShaderUniforms);
    for (const std::vector<uint32_t>& bin : bins_) {
        bytes += bin.capacity() * sizeof(uint32_t) + sizeof(std::vector<uint32_t>);
    }
    return bytes;
}

void Rasterizer::flush() {
    if (triangles_.empty()) return;

    const uint64_t tileTotal = tileCount();
    core::ThreadPool* pool = config_.enableThreads ? pool_ : nullptr;

    if (pool != nullptr && pool->size() > 0) {
        pool->parallelFor(tileTotal, 1, [&](uint64_t begin, uint64_t end) {
            uint64_t tested = 0;
            uint64_t shaded = 0;
            for (uint64_t tile = begin; tile < end; ++tile) {
                rasterizeTile(tile, tested, shaded);
            }
            if (tested != 0) fragmentsTested_.fetch_add(tested, std::memory_order_relaxed);
            if (shaded != 0) fragmentsShaded_.fetch_add(shaded, std::memory_order_relaxed);
        });
    } else {
        uint64_t tested = 0;
        uint64_t shaded = 0;
        for (uint64_t tile = 0; tile < tileTotal; ++tile) {
            rasterizeTile(tile, tested, shaded);
        }
        fragmentsTested_.fetch_add(tested, std::memory_order_relaxed);
        fragmentsShaded_.fetch_add(shaded, std::memory_order_relaxed);
    }

    counters_.fragmentsTested = fragmentsTested_.load(std::memory_order_relaxed);
    counters_.fragmentsShaded = fragmentsShaded_.load(std::memory_order_relaxed);
}

}  // namespace nova::rasterizer
