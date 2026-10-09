// NOVA GPU - Software GPU
// src/lighting/light.h - light and material types.
//
// Plain data only: the shading maths lives in `lighting.h`. Lights are passed to
// shaders through `shader::ShaderUniforms`, so a fragment shader only has to
// describe the surface it is shading.
#pragma once

#include "math/vec.h"

namespace nova::lighting {

// Constant illumination from every direction.
struct AmbientLight {
    math::Vec3 color{0.10f, 0.10f, 0.12f};
    float intensity = 1.0f;
};

// Infinitely distant light: the direction is the same everywhere, so there is no
// attenuation and no position. `direction` is the direction the light travels,
// so -direction points towards the light.
struct DirectionalLight {
    math::Vec3 direction{0.0f, -1.0f, 0.0f};
    math::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

// How a surface responds to each illumination term. Separate strengths make it
// possible to build the Phase 4 models out of one shader: `specular = 0` is
// ambient + diffuse (Lambert), `specular > 0` adds a Blinn-Phong highlight.
struct Material {
    math::Vec3 albedo{1.0f, 1.0f, 1.0f};
    float ambient = 1.0f;
    float diffuse = 1.0f;
    float specular = 0.0f;   // 0 disables the specular term entirely
    float shininess = 32.0f; // Blinn-Phong exponent; higher = tighter highlight
};

// Per-fragment surface description the lighting model needs.
struct Surface {
    math::Vec3 position{0.0f, 0.0f, 0.0f};      // world space
    math::Vec3 normal{0.0f, 0.0f, 1.0f};        // world space, normalized by shade()
    math::Vec3 viewDirection{0.0f, 0.0f, 1.0f}; // surface -> camera, unit length
};

}  // namespace nova::lighting
