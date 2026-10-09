// NOVA GPU - Software GPU
// src/gpu/device.h - the NOVA GPU API surface used by applications.
//
//   application -> NOVA GPU API -> command processor -> geometry stages ->
//   software rasterizer -> fragment stage -> depth buffer -> framebuffer
//
// Typical frame:
//     device.beginFrame(core::kDarkGray);
//     device.draw(cube, modelMatrix, camera.viewProjection(), shader);
//     device.endFrame();          // executes commands + rasterizes
//     framebuffer -> platform output, stats -> debug overlay
//
// Threading: the device owns one persistent thread pool. Geometry stages run on
// the calling thread; tile rasterization is parallelized by the rasterizer.
#pragma once

#include <chrono>

#include "core/config.h"
#include "core/framebuffer.h"
#include "core/thread_pool.h"
#include "debug/stats.h"
#include "gpu/command.h"
#include "gpu/command_processor.h"
#include "rasterizer/rasterizer.h"
#include "shader/transform_vertex_shader.h"

namespace nova::gpu {

class Device {
public:
    explicit Device(const core::RenderConfig& config = core::RenderConfig{});
    ~Device() = default;

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // --- device queries ------------------------------------------------------
    const core::RenderConfig& config() const { return config_; }
    core::Framebuffer& framebuffer() { return framebuffer_; }
    const core::Framebuffer& framebuffer() const { return framebuffer_; }
    const debug::RenderStats& stats() const { return stats_; }

    // Threads taking part in rasterization (workers + the calling thread).
    int threadCount() const { return threadCount_; }
    int tileSize() const { return rasterizer_.tileSize(); }
    int tilesX() const { return rasterizer_.tilesX(); }
    int tilesY() const { return rasterizer_.tilesY(); }

    // Bytes used by the framebuffer plus the rasterizer's per-frame working set.
    size_t workingSetBytes() const { return framebuffer_.memoryBytes() + rasterizer_.workingSetBytes(); }

    // --- frame API -----------------------------------------------------------
    void beginFrame(const core::Color& clearColor = core::kDarkGray, float clearDepthValue = 1.0f);
    void clear(const core::Color& clearColor, float clearDepthValue = 1.0f);

    // Records a draw command. The mesh and shaders must stay alive until
    // endFrame() (which is the same frame, so stack/static storage is fine).
    // This overload uses the built-in TransformVertexShader.
    void draw(const scene::Mesh& mesh, const math::Mat4& model, const math::Mat4& viewProjection,
              const shader::FragmentShader& fragmentShader);

    // Same, with an application-supplied vertex shader.
    void draw(const scene::Mesh& mesh, const math::Mat4& model, const math::Mat4& viewProjection,
              const shader::VertexShader& vertexShader,
              const shader::FragmentShader& fragmentShader);

    // --- shader state --------------------------------------------------------
    // Captured into every draw command recorded after the call. Defaults are a
    // camera at the origin and a light pointing straight down.
    void setCameraPosition(const math::Vec3& position) { cameraPosition_ = position; }
    const math::Vec3& cameraPosition() const { return cameraPosition_; }
    void setLight(const lighting::DirectionalLight& light) { light_ = light; }
    const lighting::DirectionalLight& light() const { return light_; }
    void setAmbient(const lighting::AmbientLight& ambient) { ambient_ = ambient; }
    const lighting::AmbientLight& ambient() const { return ambient_; }
    void setTime(float seconds) { time_ = seconds; }
    float time() const { return time_; }

    // Uniforms that the next recorded draw will receive (also usable by
    // application code that wants to inspect the constant block).
    shader::ShaderUniforms makeUniforms(const math::Mat4& model,
                                        const math::Mat4& viewProjection) const;

    // Executes the recorded commands and rasterizes. Consumes the command list.
    void endFrame();

    // --- instrumentation -----------------------------------------------------
    // When disabled, stage timers and FPS tracking are skipped (counters stay on).
    void setTimingEnabled(bool enabled) { timingEnabled_ = enabled; }
    bool timingEnabled() const { return timingEnabled_; }
    void resetStats();

private:
    core::RenderConfig config_;
    core::Framebuffer framebuffer_;
    core::ThreadPool pool_;
    rasterizer::Rasterizer rasterizer_;
    CommandProcessor processor_;

    CommandList commands_;
    debug::RenderStats stats_;

    // Used by draw() when the application supplies only a fragment shader.
    shader::TransformVertexShader defaultVertexShader_;
    math::Vec3 cameraPosition_{0.0f, 0.0f, 0.0f};
    lighting::AmbientLight ambient_;
    lighting::DirectionalLight light_;
    float time_ = 0.0f;

    int threadCount_ = 1;
    bool timingEnabled_ = true;

    std::chrono::steady_clock::time_point frameStart_{};
    double frameMsEma_ = 0.0;
    bool emaPrimed_ = false;
};

}  // namespace nova::gpu
