// NOVA GPU - Software GPU
// src/shader/uniforms.h - the per-draw constant block handed to shaders.
//
// This is NOVA's stand-in for a GPU constant buffer: plain data, filled once per
// draw call, readable by both shader stages. Adding a field here makes it
// available to every shader without touching the pipeline, which is what keeps
// the shader system extensible without a shader compiler.
#pragma once

#include "lighting/light.h"
#include "math/mat4.h"
#include "math/vec.h"

namespace nova::shader {

// The lights a shader can see. Phase 4 ships one ambient and one directional
// light; growing to a light array means changing this block and the lighting
// function, not the shader interface.
struct Lights {
    lighting::AmbientLight ambient;
    lighting::DirectionalLight directional;
};

// Per-draw uniforms. The command processor fills these from the draw command;
// the vertex stage reads the matrices, the fragment stage typically reads the
// camera position, the lights and time.
struct ShaderUniforms {
    math::Mat4 model = math::Mat4::identity();           // model -> world
    math::Mat4 viewProjection = math::Mat4::identity();  // world -> clip
    math::Vec3 cameraPosition{0.0f, 0.0f, 0.0f};         // world space
    Lights lights;
    float time = 0.0f;  // seconds since the first frame (animation input)
};

}  // namespace nova::shader
