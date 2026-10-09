// NOVA GPU - Software GPU
// src/shader/phong_shader.h - textured, lit material shader (Phases 3 + 4).
//
// Combines the two phases in one shader the way a real pipeline does:
//   1. albedo = (texture sample, if bound) * material albedo * vertex color
//   2. color  = lighting::shade(ambient + directional diffuse + Blinn-Phong specular)
//
// The specular term is what Phase 4 calls "later"; it is already here because it
// costs one pow() per fragment and shares the halfway vector with the diffuse
// term - but `Material::specular = 0` gives the plain Lambert model.
#pragma once

#include "lighting/lighting.h"
#include "shader/fragment_shader.h"
#include "texture/texture.h"

namespace nova::shader {

class PhongFragmentShader final : public FragmentShader {
public:
    PhongFragmentShader() { material_.specular = 0.30f; }
    explicit PhongFragmentShader(const lighting::Material& material) : material_(material) {}

    // Optional albedo texture. Pass nullptr to shade the material albedo alone.
    void setAlbedoTexture(const texture::Texture* texture, texture::Sampler sampler = {}) {
        albedoTexture_ = texture;
        sampler_ = sampler;
    }

    void setMaterial(const lighting::Material& material) { material_ = material; }
    const lighting::Material& material() const { return material_; }

    math::Vec4 shade(const FragmentInput& input, const ShaderUniforms& uniforms) const override {
        lighting::Material material = material_;
        material.albedo = material.albedo * input.varying.color;
        if (albedoTexture_ != nullptr) {
            const math::Vec4 texel = albedoTexture_->sample(input.varying.uv, sampler_);
            material.albedo = material.albedo * math::Vec3{texel.x, texel.y, texel.z};
        }

        lighting::Surface surface;
        surface.position = input.varying.worldPosition;
        surface.normal = input.varying.normal;
        surface.viewDirection =
            math::normalize(uniforms.cameraPosition - input.varying.worldPosition);

        const math::Vec3 c = lighting::shade(material, uniforms.lights.ambient,
                                             uniforms.lights.directional, surface);
        return {c.x, c.y, c.z, 1.0f};
    }

    const char* name() const override { return "PhongFragmentShader"; }

private:
    lighting::Material material_{};
    const texture::Texture* albedoTexture_ = nullptr;
    texture::Sampler sampler_{};
};

}  // namespace nova::shader
