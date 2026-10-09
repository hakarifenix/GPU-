#include "pipeline/vertex_processor.h"

namespace nova::pipeline {

void processVertices(const core::Vertex* vertices, size_t count, const shader::VertexShader& shader,
                     const shader::ShaderUniforms& uniforms, ClipVertex* out) {
    shader::VertexOutput output;
    for (size_t i = 0; i < count; ++i) {
        shader.shade(vertices[i], uniforms, output);
        out[i].clip = output.clip;
        out[i].varying = output.varying;
    }
}

}  // namespace nova::pipeline
