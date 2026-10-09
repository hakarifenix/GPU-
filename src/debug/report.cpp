#include "debug/report.h"

#include <cstdio>

#include "platform/console.h"

namespace nova::debug {
namespace {

std::string megabytes(size_t bytes) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.2f MiB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    return buffer;
}

}  // namespace

void printStats(const RenderStats& stats, const FrameReportContext& context) {
    using nova::platform::formatThousands;

    std::printf("%s\n", stats.renderer.c_str());
    std::printf("  frame       : %8.2f ms   (%6.1f FPS)\n", stats.timings.frameMs, stats.fps);
    std::printf("  cull        : %8.3f ms   (%s objects culled)\n", stats.timings.cullMs,
                formatThousands(stats.counters.objectsCulled).c_str());
    std::printf("  vertex      : %8.3f ms\n", stats.timings.vertexMs);
    std::printf("  clip+setup  : %8.3f ms   (clipped %s tris)\n", stats.timings.setupMs,
                formatThousands(stats.counters.trianglesClipped).c_str());
    std::printf("  raster+frag : %8.3f ms\n", stats.timings.rasterMs);
    std::printf("  triangles   : %s submitted, %s visible, %s culled (frustum %s)\n",
                formatThousands(stats.counters.trianglesSubmitted).c_str(),
                formatThousands(stats.counters.trianglesRasterized).c_str(),
                formatThousands(stats.counters.trianglesCulled).c_str(),
                formatThousands(stats.counters.trianglesFrustumCulled).c_str());
    std::printf("  objects     : %s submitted, %s culled\n",
                formatThousands(stats.counters.objectsSubmitted).c_str(),
                formatThousands(stats.counters.objectsCulled).c_str());
    std::printf("  fragments   : %s tested, %s shaded\n",
                formatThousands(stats.counters.fragmentsTested).c_str(),
                formatThousands(stats.counters.fragmentsShaded).c_str());
    std::printf("  threads     : %d   tiles: %dx%d @ %d px (%s tile-triangle pairs)\n",
                stats.threadCount, context.tilesX, context.tilesY, context.tileSize,
                formatThousands(stats.counters.tileTrianglePairs).c_str());
    std::printf("  shaders     : %s + %s\n", stats.vertexShader.c_str(),
                stats.fragmentShader.c_str());
    std::printf("  memory      : framebuffer+working set %s, geometry %s\n",
                megabytes(context.workingSetBytes).c_str(), megabytes(context.meshBytes).c_str());
}

void printStatsLine(const RenderStats& stats) {
    std::printf("%10.2f %8.2f %8.3f %8.3f %8.3f %10s %10s %8s\n", stats.timings.frameMs, stats.fps,
                stats.timings.vertexMs, stats.timings.setupMs, stats.timings.rasterMs,
                nova::platform::formatThousands(stats.counters.trianglesSubmitted).c_str(),
                nova::platform::formatThousands(stats.counters.fragmentsShaded).c_str(),
                nova::platform::formatThousands(stats.counters.trianglesRasterized).c_str());
}

void printStatsHeader() {
    std::printf("%10s %8s %8s %8s %8s %10s %10s %8s\n", "frame ms", "FPS", "vertex ms",
                "setup ms", "raster ms", "tris", "frags", "visible");
}

}  // namespace nova::debug
