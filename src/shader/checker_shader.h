// NOVA GPU - Software GPU
// src/shader/checker_shader.h - procedural checkerboard driven by the uv varying.
//
// Phase 2 uses it to prove interpolated texture coordinates reach the fragment
// stage end to end. Phase 3 replaces it with real texture sampling, nearest and
// bilinear filtering and mipmaps.
#pragma once

#include <cmath>

#include "shader/fragment_shader.h"

namespace nova::shader {

class CheckerFragmentShader final : public FragmentShader {
public:
    explicit CheckerFragmentShader(float cellsPerUnit = 4.0f,
                                   const math::Vec3& light = {0.92f, 0.92f, 0.95f},
                                   const math::Vec3& dark = {0.16f, 0.16f, 0.22f})
        : cells_(cellsPerUnit), light_(light), dark_(dark) {}

    math::Vec4 shade(const FragmentInput& input, const ShaderUniforms& /*uniforms*/) const override {
        const float cx = std::floor(input.varying.uv.x * cells_);
        const float cy = std::floor(input.varying.uv.y * cells_);
        const bool odd = (static_cast<int>(cx) + static_cast<int>(cy)) % 2 != 0;
        // Tile color is modulated by the interpolated vertex color so the two
        // attribute streams are visibly independent.
        const math::Vec3 tile = odd ? dark_ : light_;
        const math::Vec3 c = tile * input.varying.color;
        return {c.x, c.y, c.z, 1.0f};
    }

    const char* name() const override { return "CheckerFragmentShader"; }

private:
    float cells_;
    math::Vec3 light_;
    math::Vec3 dark_;
};

}  // namespace nova::shader
