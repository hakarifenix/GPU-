// NOVA GPU - Software GPU
// benchmarks/benchmark_vertex.cpp - isolated vertex-stage measurement.
//
// Why this exists: frame times on a shared/noisy machine cannot resolve a change
// that moves 8% of the frame. This benchmark measures only the vertex stage
// (processVertices) over many repetitions and reports the best and median time,
// so the SIMD fast path can be judged on its own instead of through wall-clock
// frame noise. Run it on the target machine to decide whether SIMD is worth it
// (see docs/performance.md).
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/vertex.h"
#include "debug/stats.h"
#include "math/mat4.h"
#include "math/math_utils.h"
#include "pipeline/clip_vertex.h"
#include "pipeline/vertex_processor.h"
#include "platform/console.h"
#include "scene/shapes.h"
#include "shader/transform_vertex_shader.h"

using namespace nova;

namespace {

struct Result {
    double bestNsPerVertex = 0.0;
    double medianNsPerVertex = 0.0;
};

Result measure(size_t vertexCount, bool simd, int iterations) {
    // A realistic stream: the vertices of a scaled cube lattice, rotated.
    const scene::Mesh mesh = scene::makeSphere(1.0f, 64, 32);
    std::vector<core::Vertex> vertices(mesh.vertices.begin(), mesh.vertices.end());
    while (vertices.size() < vertexCount) {
        const size_t before = vertices.size();
        for (size_t i = 0; i < before && vertices.size() < vertexCount; ++i) {
            vertices.push_back(mesh.vertices[i % mesh.vertexCount()]);
        }
    }
    vertices.resize(vertexCount);

    std::vector<pipeline::ClipVertex> output(vertexCount);

    shader::ShaderUniforms uniforms;
    uniforms.model = math::rotationY(math::radians(30.0f)) * math::rotationX(math::radians(12.0f));
    uniforms.viewProjection = math::perspective(math::radians(60.0f), 1.5f, 0.5f, 200.0f) *
                              math::lookAt({0.0f, 0.0f, 30.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    uniforms.simd = simd;

    const shader::TransformVertexShader shader;
    std::vector<double> samples;
    samples.reserve(static_cast<size_t>(iterations));

    for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto start = std::chrono::steady_clock::now();
        pipeline::processVertices(vertices.data(), vertices.size(), shader, uniforms, output.data());
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        samples.push_back(ms * 1.0e6 / static_cast<double>(vertexCount));  // ns per vertex
    }

    std::sort(samples.begin(), samples.end());
    Result result;
    result.bestNsPerVertex = samples.front();
    result.medianNsPerVertex = samples[samples.size() / 2];
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    platform::enableAnsiColors();

    size_t vertexCount = 200000;
    int iterations = 40;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--vertices") == 0 && i + 1 < argc) {
            vertexCount = static_cast<size_t>(std::atol(argv[++i]));
        } else if (std::strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            iterations = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("NOVA GPU vertex-stage benchmark\n"
                        "  --vertices N    vertices per iteration (default 200000)\n"
                        "  --iterations N  repetitions (default 40)\n");
            return 0;
        }
    }

    std::printf("%s\n", debug::rendererName());
    std::printf("vertex stage: %zu vertices per iteration, %d iterations, best/median of run\n\n",
                vertexCount, iterations);

    const Result scalar = measure(vertexCount, false, iterations);
    const Result simd = measure(vertexCount, true, iterations);

    std::printf("%10s %16s %16s\n", "variant", "best ns/vertex", "median ns/vertex");
    std::printf("%10s %16.2f %16.2f\n", "scalar", scalar.bestNsPerVertex, scalar.medianNsPerVertex);
    std::printf("%10s %16.2f %16.2f\n", "simd", simd.bestNsPerVertex, simd.medianNsPerVertex);
    const double speedup = scalar.bestNsPerVertex / (simd.bestNsPerVertex > 0.0
                                                        ? simd.bestNsPerVertex
                                                        : 1.0);
    std::printf("\nsimd speedup on this machine: %.2fx (best case)\n", speedup);
    std::printf("note: --compare in nova_benchmark measures the same switch through whole frames,\n"
                "      where the vertex stage is only a few percent of the total.\n");
    return 0;
}
