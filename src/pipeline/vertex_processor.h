// NOVA GPU - Software GPU
// src/pipeline/vertex_processor.h - stage 2: per-vertex shader execution.
//
// Runs the draw's vertex shader over the mesh's vertex stream, producing
// clip-space vertices (plus varyings) for clipping and triangle setup. The
// transform itself lives in the shader, not here: this stage is just the loop.
#pragma once

#include <cstddef>

#include "core/vertex.h"
#include "pipeline/clip_vertex.h"
#include "shader/vertex_shader.h"

namespace nova::pipeline {

// `shader` and `uniforms` come from the draw command; `out` must hold at least
// `count` ClipVertex slots.
void processVertices(const core::Vertex* vertices, size_t count, const shader::VertexShader& shader,
                     const shader::ShaderUniforms& uniforms, ClipVertex* out);

}  // namespace nova::pipeline
