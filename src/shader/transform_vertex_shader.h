// NOVA GPU - Software GPU
// src/shader/transform_vertex_shader.h - the default vertex shader.
#pragma once

#include "shader/vertex_shader.h"

namespace nova::shader {

// Object space -> clip space through the draw's model and view-projection
// matrices, carrying world position, world normal, vertex color and uv into the
// varyings. This is the shader the device uses when an application does not
// supply one, so simple draws (and Phase 1 code) keep working unchanged.
//
// Normals go through the model matrix, which is exact for rotation and uniform
// scale - all transforms NOVA's demos use. Non-uniform scale would need the
// inverse transpose; that is deliberately left to a later phase.
class TransformVertexShader final : public VertexShader {
public:
    void shade(const core::Vertex& in, const ShaderUniforms& uniforms,
               VertexOutput& out) const override {
        const math::Mat4& model = uniforms.model;
        out.clip = math::transform(uniforms.viewProjection * model, math::Vec4{in.position, 1.0f});
        out.varying.worldPosition = math::transformPoint(model, in.position);
        out.varying.normal = math::transformDirection(model, in.normal);
        out.varying.color = in.color;
        out.varying.uv = in.uv;
    }

    const char* name() const override { return "TransformVertexShader"; }
};

}  // namespace nova::shader
