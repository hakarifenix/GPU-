// NOVA GPU - Software GPU
// src/shader/color_shader.h - unlit shader: the interpolated vertex color.
#pragma once

#include "shader/fragment_shader.h"

namespace nova::shader {

// Passes the interpolated vertex color straight through, i.e. Gouraud shading
// with no lighting. This is the Phase 1 shader and the baseline the Phase 2
// shader system is checked against.
class ColorFragmentShader final : public FragmentShader {
public:
    math::Vec4 shade(const FragmentInput& input, const ShaderUniforms& /*uniforms*/) const override {
        return {input.varying.color.x, input.varying.color.y, input.varying.color.z, 1.0f};
    }

    const char* name() const override { return "ColorFragmentShader"; }
};

}  // namespace nova::shader
