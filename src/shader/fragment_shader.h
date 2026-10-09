// NOVA GPU - Software GPU
// src/shader/fragment_shader.h - the fragment (pixel) stage interface.
//
// The rasterizer hands a fragment to a shader and receives a color. Attributes
// in `FragmentInput::varying` are already perspective-correctly interpolated, so
// a shader never has to think about the perspective divide.
#pragma once

#include "math/vec.h"
#include "shader/uniforms.h"
#include "shader/varying.h"

namespace nova::shader {

// Per-fragment inputs produced by the rasterizer.
struct FragmentInput {
    math::Vec2 screenPos;  // pixel-center coordinates
    float depth = 0.0f;    // window-space depth in [0, 1]
    Varying varying;       // perspective-correct interpolated attributes
};

class FragmentShader {
public:
    virtual ~FragmentShader() = default;

    // Returns the fragment color; components are clamped to [0, 1] on write.
    virtual math::Vec4 shade(const FragmentInput& input, const ShaderUniforms& uniforms) const = 0;

    // Human-readable name reported by the debug output.
    virtual const char* name() const { return "fragment shader"; }
};

}  // namespace nova::shader
