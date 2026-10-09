#include "gpu/device.h"

#include <algorithm>

namespace nova::gpu {

Device::Device(const core::RenderConfig& config)
    : config_(config),
      framebuffer_(config.width, config.height),
      pool_(config.enableThreads ? config.threadCount : 1),
      rasterizer_(config_, framebuffer_, &pool_),
      processor_(config_, config_.width, config_.height, rasterizer_) {
    threadCount_ = (config_.enableThreads && pool_.size() > 0) ? static_cast<int>(pool_.size()) + 1 : 1;

    stats_.width = config_.width;
    stats_.height = config_.height;
    stats_.framebufferBytes = framebuffer_.memoryBytes();
    stats_.threadCount = threadCount_;
    stats_.counters.pixelsPerFrame = static_cast<uint64_t>(framebuffer_.pixelCount());
}

void Device::resetStats() {
    const debug::RenderStats fresh;
    stats_.timings = fresh.timings;
    stats_.counters = fresh.counters;
    stats_.counters.pixelsPerFrame = static_cast<uint64_t>(framebuffer_.pixelCount());
    stats_.fps = 0.0;
    frameMsEma_ = 0.0;
    emaPrimed_ = false;
}

void Device::clear(const core::Color& clearColor, float clearDepthValue) {
    if (config_.clearColor) framebuffer_.clearColor(clearColor);
    if (config_.clearDepth) framebuffer_.clearDepth(clearDepthValue);
}

void Device::beginFrame(const core::Color& clearColor, float clearDepthValue) {
    if (timingEnabled_) frameStart_ = std::chrono::steady_clock::now();

    commands_.clear();
    rasterizer_.beginFrame();

    stats_.timings = debug::StageTimings{};
    stats_.counters = debug::RenderCounters{};
    stats_.counters.pixelsPerFrame = static_cast<uint64_t>(framebuffer_.pixelCount());

    clear(clearColor, clearDepthValue);
}

shader::ShaderUniforms Device::makeUniforms(const math::Mat4& model,
                                             const math::Mat4& viewProjection) const {
    shader::ShaderUniforms uniforms;
    uniforms.model = model;
    uniforms.viewProjection = viewProjection;
    uniforms.cameraPosition = cameraPosition_;
    uniforms.lights.ambient = ambient_;
    uniforms.lights.directional = light_;
    uniforms.time = time_;
    return uniforms;
}

void Device::draw(const scene::Mesh& mesh, const math::Mat4& model,
                  const math::Mat4& viewProjection, const shader::FragmentShader& fragmentShader) {
    draw(mesh, model, viewProjection, defaultVertexShader_, fragmentShader);
}

void Device::draw(const scene::Mesh& mesh, const math::Mat4& model,
                  const math::Mat4& viewProjection, const shader::VertexShader& vertexShader,
                  const shader::FragmentShader& fragmentShader) {
    Command command;
    command.draw.mesh = &mesh;
    command.draw.vertexShader = &vertexShader;
    command.draw.fragmentShader = &fragmentShader;
    command.draw.uniforms = makeUniforms(model, viewProjection);
    commands_.push_back(command);
}

void Device::endFrame() {
    processor_.execute(commands_, stats_.timings, stats_.counters);

    const auto rasterBegin = std::chrono::steady_clock::now();
    rasterizer_.flush();
    stats_.timings.rasterMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rasterBegin)
            .count();

    const debug::RenderCounters& rasterCounters = rasterizer_.counters();
    stats_.counters.trianglesRasterized = rasterCounters.trianglesRasterized;
    stats_.counters.fragmentsTested = rasterCounters.fragmentsTested;
    stats_.counters.fragmentsShaded = rasterCounters.fragmentsShaded;

    if (timingEnabled_) {
        stats_.timings.frameMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStart_)
                .count();
        // Exponential moving average smooths the FPS readout without hiding
        // sustained regressions (which is what a benchmark needs).
        constexpr double kAlpha = 0.1;
        if (!emaPrimed_) {
            frameMsEma_ = stats_.timings.frameMs;
            emaPrimed_ = true;
        } else {
            frameMsEma_ += kAlpha * (stats_.timings.frameMs - frameMsEma_);
        }
        stats_.fps = frameMsEma_ > 0.0 ? 1000.0 / frameMsEma_ : 0.0;
    }

    commands_.clear();
}

}  // namespace nova::gpu
