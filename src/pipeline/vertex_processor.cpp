#include "pipeline/vertex_processor.h"

namespace nova::pipeline {

void processVertices(const core::Vertex* vertices, size_t count, const shader::VertexShader& shader,
                     const shader::ShaderUniforms& uniforms, ClipVertex* out) {
    // ClipVertex derives from shader::VertexOutput, so the shader writes directly
    // into the pipeline's vertex array - no intermediate buffer, no copy.
    shader.shadeBatch(vertices, count, uniforms, out);
}

}  // namespace nova::pipeline
