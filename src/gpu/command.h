// NOVA GPU - Software GPU
// src/gpu/command.h - commands recorded by the NOVA GPU API.
//
// The application records commands; the command processor executes them. Keeping
// the two apart is what makes the pipeline stages measurable in isolation and
// lets later phases add batching/reordering without touching application code.
#pragma once

#include <vector>

#include "math/mat4.h"
#include "scene/mesh.h"
#include "shader/fragment_shader.h"
#include "shader/uniforms.h"
#include "shader/vertex_shader.h"

namespace nova::gpu {

struct DrawCommand {
    const scene::Mesh* mesh = nullptr;
    const shader::VertexShader* vertexShader = nullptr;
    const shader::FragmentShader* fragmentShader = nullptr;
    // Constant block for this draw: matrices, camera and light. Captured when
    // the command is recorded, so the executed frame is independent of any
    // later state changes.
    shader::ShaderUniforms uniforms{};

    bool valid() const {
        return mesh != nullptr && vertexShader != nullptr && fragmentShader != nullptr &&
               !mesh->empty();
    }
};

struct Command {
    DrawCommand draw;
};

using CommandList = std::vector<Command>;

}  // namespace nova::gpu
