// NOVA GPU - Software GPU
// src/shader/vertex_shader.h - the vertex stage interface.
//
// A shader is an ordinary C++ class: no GLSL, no runtime compilation. The
// pipeline calls shade() once per vertex with the vertex as stored in the mesh
// (model space) and the draw's uniforms, and gets back a clip-space position
// plus the varyings the rest of the pipeline carries forward.
#pragma once

#include "core/vertex.h"
#include "math/vec.h"
#include "shader/uniforms.h"
#include "shader/varying.h"

namespace nova::shader {

// Output of the vertex stage.
struct VertexOutput {
    math::Vec4 clip{0.0f, 0.0f, 0.0f, 1.0f};  // homogeneous clip space
    Varying varying;                          // interpolated by the rasterizer
};

class VertexShader {
public:
    virtual ~VertexShader() = default;

    // Writes `out` for one input vertex. Taking the output by reference keeps
    // the per-vertex hot loop allocation-free.
    virtual void shade(const core::Vertex& in, const ShaderUniforms& uniforms,
                       VertexOutput& out) const = 0;

    // Human-readable name reported by the debug output.
    virtual const char* name() const { return "vertex shader"; }
};

}  // namespace nova::shader
