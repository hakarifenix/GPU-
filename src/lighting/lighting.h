// NOVA GPU - Software GPU
// src/lighting/lighting.h - CPU-friendly shading functions.
//
// One function covers the whole Phase 4 model:
//   ambient  = albedo * ambient.color * (ambient.intensity * material.ambient)
//   diffuse  = albedo * light.color * (light.intensity * material.diffuse * max(N.L, 0))
//   specular =            light.color * (light.intensity * material.specular * pow(N.H, shininess))
//
// with L pointing towards the light and H the Blinn-Phong halfway vector between
// L and the view direction. The specular term is only evaluated when the
// material asks for it, so a Lambert surface costs two dot products.
#pragma once

#include "lighting/light.h"
#include "math/vec.h"

namespace nova::lighting {

// Combines the ambient and directional terms. The result is not clamped: values
// above 1 are allowed and clamped when written to the framebuffer, exactly like
// a GPU with an unclamped float target.
math::Vec3 shade(const Material& material, const AmbientLight& ambient,
                 const DirectionalLight& light, const Surface& surface);

}  // namespace nova::lighting
