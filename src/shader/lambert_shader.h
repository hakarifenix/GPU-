// NOVA GPU - Software GPU
// src/shader/lambert_shader.h - ambient + directional diffuse lighting.
//
// Per-fragment lighting built from the interpolated world normal, the world
// position and the draw's lights: the classic Lambert model with an ambient term
// and no specular. The maths lives in src/lighting so the same model is shared
// with the Phong shader and the unit tests.
#pragma once

#include "lighting/lighting.h"
#include "shader/fragment_shader.h"

namespace nova::shader {

class LambertFragmentShader final : public FragmentShader {
public:
    math::Vec4 shade(const FragmentInput& input, const ShaderUniforms& uniforms) const override {
        lighting::Material material;
        material.albedo = input.varying.color;
        material.specular = 0.0f;  // Lambert: no highlight

        lighting::Surface surface;
        surface.position = input.varying.worldPosition;
        surface.normal = input.varying.normal;
        surface.viewDirection = math::normalize(uniforms.cameraPosition - input.varying.worldPosition);

        const math::Vec3 c = lighting::shade(material, uniforms.lights.ambient,
                                             uniforms.lights.directional, surface);
        return {c.x, c.y, c.z, 1.0f};
    }

    const char* name() const override { return "LambertFragmentShader"; }
};

}  // namespace nova::shader
