// NOVA GPU - Software GPU
// src/gpu/command_processor.h - stage 1 of the pipeline: command processing.
//
// Walks the recorded command list and drives the geometry stages:
//   vertex processing -> clipping -> triangle setup -> rasterizer submission.
//
// Timing note: the clip stage is timed only for the triangles that actually
// cross a clip plane. Fully visible geometry skips the clipper entirely (the
// fast path just checks the three vertices), so clipMs is normally 0.0 and the
// timestamp overhead stays off the hot path.
#pragma once

#include "core/config.h"
#include "core/framebuffer.h"
#include "debug/stats.h"
#include "gpu/command.h"
#include "pipeline/clip_vertex.h"
#include "pipeline/viewport.h"
#include "rasterizer/rasterizer.h"

namespace nova::gpu {

class CommandProcessor {
public:
    CommandProcessor(const core::RenderConfig& config, int framebufferWidth, int framebufferHeight,
                     rasterizer::Rasterizer& rasterizer);

    // Executes the whole list, accumulating stage timings and counters.
    void execute(const CommandList& commands, debug::StageTimings& timings,
                 debug::RenderCounters& counters);

private:
    void executeDraw(const DrawCommand& command, debug::StageTimings& timings,
                     debug::RenderCounters& counters);

    const core::RenderConfig& config_;
    rasterizer::Rasterizer& rasterizer_;
    pipeline::Viewport viewport_;

    // Reused across draws (and across frames) so drawing never allocates.
    std::vector<pipeline::ClipVertex> vertexStage_;
    std::vector<pipeline::ClipVertex> clippedVertices_;
};

}  // namespace nova::gpu
