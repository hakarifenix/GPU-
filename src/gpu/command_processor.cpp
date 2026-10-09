#include "gpu/command_processor.h"

#include <chrono>

#include "math/bounds.h"
#include "pipeline/clipper.h"
#include "pipeline/frustum.h"
#include "pipeline/triangle_setup.h"
#include "pipeline/vertex_processor.h"

namespace nova::gpu {
namespace {

using Clock = std::chrono::steady_clock;

double millisSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// True when the triangle lies entirely inside both clip planes and needs no
// clipping at all. This is the common case for geometry in front of the camera.
bool needsClipping(const pipeline::ClipVertex in[3], bool clipNearPlane) {
    for (int i = 0; i < 3; ++i) {
        const math::Vec4& c = in[i].clip;
        if (c.w <= 0.0f) return true;
        if (clipNearPlane && (c.z + c.w) < 0.0f) return true;
    }
    return false;
}

}  // namespace

CommandProcessor::CommandProcessor(const core::RenderConfig& config, int framebufferWidth,
                                   int framebufferHeight, rasterizer::Rasterizer& rasterizer)
    : config_(config), rasterizer_(rasterizer) {
    viewport_ = pipeline::Viewport::forFramebuffer(framebufferWidth, framebufferHeight);
    // clipTriangle() needs a 4-vertex destination; allocate it once here so the
    // hot loop never writes into an empty vector's null data pointer.
    clippedVertices_.resize(4);
}

void CommandProcessor::execute(const CommandList& commands, debug::StageTimings& timings,
                               debug::RenderCounters& counters) {
    for (const Command& command : commands) {
        if (!command.draw.valid()) continue;
        executeDraw(command.draw, timings, counters);
    }
}

void CommandProcessor::executeDraw(const DrawCommand& command, debug::StageTimings& timings,
                                   debug::RenderCounters& counters) {
    const scene::Mesh& mesh = *command.mesh;
    const size_t vertexCount = mesh.vertexCount();
    const size_t triangleCount = mesh.triangleCount();

    ++counters.drawCalls;
    ++counters.objectsSubmitted;
    counters.trianglesSubmitted += triangleCount;

    // --- object visibility (frustum culling) ---------------------------------
    // Reject the whole draw when its world bounds miss the frustum: no vertices
    // are transformed and no triangles are binned, which is why this pays for
    // itself on any object with more than a handful of triangles.
    if (config_.frustumCulling) {
        const auto cullBegin = Clock::now();
        const pipeline::Frustum frustum =
            pipeline::Frustum::fromViewProjection(command.uniforms.viewProjection);
        const math::Aabb worldBounds = mesh.bounds().transformed(command.uniforms.model);
        const bool visible = frustum.intersects(worldBounds);
        timings.cullMs += millisSince(cullBegin);
        if (!visible) {
            ++counters.objectsCulled;
            counters.trianglesFrustumCulled += triangleCount;
            return;
        }
    }

    counters.verticesProcessed += vertexCount;

    // One constant block per draw, shared by every triangle it produces.
    const uint32_t uniformBlock = rasterizer_.addUniformBlock(command.uniforms);

    // --- vertex processing (runs the draw's vertex shader) -------------------
    const auto vertexBegin = Clock::now();
    vertexStage_.resize(vertexCount);
    pipeline::processVertices(mesh.vertices.data(), vertexCount, *command.vertexShader,
                              command.uniforms, vertexStage_.data());
    timings.vertexMs += millisSince(vertexBegin);

    // --- clipping + triangle setup ------------------------------------------
    const auto setupBegin = Clock::now();

    for (size_t t = 0; t < triangleCount; ++t) {
        const uint32_t i0 = mesh.indices[t * 3 + 0];
        const uint32_t i1 = mesh.indices[t * 3 + 1];
        const uint32_t i2 = mesh.indices[t * 3 + 2];

        pipeline::ClipVertex triangle[3] = {vertexStage_[i0], vertexStage_[i1], vertexStage_[i2]};

        int polygonCount = 3;
        const pipeline::ClipVertex* polygon = triangle;

        if (needsClipping(triangle, config_.clipNearPlane)) {
            const auto clipBegin = Clock::now();
            polygonCount = pipeline::clipTriangle(triangle, config_.clipNearPlane, clippedVertices_.data());
            timings.clipMs += millisSince(clipBegin);
            polygon = clippedVertices_.data();
            if (polygonCount == 4) ++counters.trianglesClipped;
            if (polygonCount < 3) {
                ++counters.trianglesCulled;
                continue;
            }
        }

        // A clipped triangle becomes a quad; re-emit it as two triangles.
        const int emitCount = polygonCount == 4 ? 2 : 1;
        for (int e = 0; e < emitCount; ++e) {
            pipeline::ClipVertex tri[3];
            if (emitCount == 1) {
                tri[0] = polygon[0];
                tri[1] = polygon[1];
                tri[2] = polygon[2];
            } else if (e == 0) {
                tri[0] = polygon[0];
                tri[1] = polygon[1];
                tri[2] = polygon[2];
            } else {
                tri[0] = polygon[0];
                tri[1] = polygon[2];
                tri[2] = polygon[3];
            }

            pipeline::ScreenTriangle screen;
            const pipeline::SetupResult result =
                pipeline::setupTriangle(tri, viewport_, config_.frontFaceCCW,
                                        config_.backfaceCulling, screen);
            if (result == pipeline::SetupResult::Culled) {
                ++counters.trianglesCulled;
                continue;
            }
            if (result == pipeline::SetupResult::Degenerate) {
                if (config_.cullDegenerate) ++counters.trianglesCulled;
                continue;
            }
            rasterizer_.submit(screen, *command.fragmentShader, uniformBlock);
        }
    }

    timings.setupMs += millisSince(setupBegin);
}

}  // namespace nova::gpu
