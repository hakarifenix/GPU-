// NOVA GPU - Software GPU
// src/debug/report.h - console report of a frame's render statistics.
//
// The same numbers the in-image debug overlay shows, formatted for a terminal.
// Shared by the examples and the benchmark so both report identically.
#pragma once

#include <cstddef>

#include "debug/stats.h"

namespace nova::debug {

struct FrameReportContext {
    int tileSize = 32;
    int tilesX = 0;
    int tilesY = 0;
    size_t workingSetBytes = 0;  // framebuffer + rasterizer working set
    size_t meshBytes = 0;        // geometry submitted this frame
};

// Prints the renderer name plus FPS, frame time and per-stage timings.
void printStats(const RenderStats& stats, const FrameReportContext& context);

// Compact single-line form, useful when printing many frames.
void printStatsLine(const RenderStats& stats);

// Column header matching printStatsLine().
void printStatsHeader();

}  // namespace nova::debug
