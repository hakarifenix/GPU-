// NOVA GPU - Software GPU
// src/shader/texture_shader.h - unlit texture sampling shader (Phase 3).
//
// Samples the bound texture with the interpolated uv. The texture object is held
// by the shader (the CPU equivalent of a bound texture unit) and must outlive the
// frame; the sampler state picks nearest or bilinear filtering and the wrap mode.
#pragma once

#include "shader/fragment_shader.h"
#include "texture/texture.h"

namespace nova::shader {

class TexturedFragmentShader final : public FragmentShader {
public:
    // `texture` must outlive every frame drawn with this shader.
    explicit TexturedFragmentShader(const texture::Texture& texture, texture::Sampler sampler = {})
        : texture_(&texture), sampler_(sampler) {}

    // Multiplies the texel by the interpolated vertex color (handy for tinting).
    void setModulateByVertexColor(bool enabled) { modulate_ = enabled; }

    math::Vec4 shade(const FragmentInput& input, const ShaderUniforms& /*uniforms*/) const override {
        const math::Vec4 texel = texture_->sample(input.varying.uv, sampler_);
        math::Vec3 rgb{texel.x, texel.y, texel.z};
        if (modulate_) rgb = rgb * input.varying.color;
        return {rgb.x, rgb.y, rgb.z, texel.w};
    }

    const char* name() const override { return "TexturedFragmentShader"; }

private:
    const texture::Texture* texture_;
    texture::Sampler sampler_{};
    bool modulate_ = true;
};

}  // namespace nova::shader
