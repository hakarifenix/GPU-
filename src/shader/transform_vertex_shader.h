// NOVA GPU - Software GPU
// src/shader/transform_vertex_shader.h - the default vertex shader.
#pragma once

#include "math/simd.h"
#include "shader/vertex_shader.h"

namespace nova::shader {

// Object space -> clip space through the draw's model and view-projection
// matrices, carrying world position, world normal, vertex color and uv into the
// varyings. This is the shader the device uses when an application does not
// supply one, so simple draws (and Phase 1 code) keep working unchanged.
//
// Normals go through the model matrix, which is exact for rotation and uniform
// scale - all transforms NOVA's demos use. Non-uniform scale would need the
// inverse transpose; that is deliberately left to a later phase.
class TransformVertexShader final : public VertexShader {
public:
    void shade(const core::Vertex& in, const ShaderUniforms& uniforms,
               VertexOutput& out) const override {
        shadeOne(in, uniforms.viewProjection * uniforms.model, uniforms.model, out);
    }

    // One batch per draw instead of one shade() call per vertex: the draw's
    // matrices are multiplied once, and on SSE2 the arithmetic is 4-wide. The
    // scalar batch is deliberately the same computation minus the SIMD, so
    // `uniforms.simd` is a fair arithmetic-width comparison in --compare.
    void shadeBatch(const core::Vertex* in, size_t count, const ShaderUniforms& uniforms,
                    VertexOutput* out) const override {
        const math::Mat4 mvp = uniforms.viewProjection * uniforms.model;
        const math::Mat4& model = uniforms.model;

#if defined(NOVA_HAS_SSE2)
        if (uniforms.simd) {
            shadeBatchSimd(in, count, mvp, model, out);
            return;
        }
#endif
        for (size_t i = 0; i < count; ++i) shadeOne(in[i], mvp, model, out[i]);
    }

    const char* name() const override { return "TransformVertexShader"; }

private:
    // The scalar reference implementation. shade() and the SIMD batch must agree
    // with this bit for bit, which is what the batch-equivalence test asserts.
    static void shadeOne(const core::Vertex& in, const math::Mat4& mvp, const math::Mat4& model,
                         VertexOutput& out) {
        out.clip = math::transform(mvp, math::Vec4{in.position, 1.0f});
        out.varying.worldPosition = math::transformPoint(model, in.position);
        out.varying.normal = math::transformDirection(model, in.normal);
        out.varying.color = in.color;
        out.varying.uv = in.uv;
    }

#if defined(NOVA_HAS_SSE2)
    // Four vertices at a time through the same matrices. All five fields are
    // written for every vertex, so no output is left holding an earlier draw's
    // varyings (the contract the interface states for batch overrides).
    static void shadeBatchSimd(const core::Vertex* in, size_t count, const math::Mat4& mvp,
                               const math::Mat4& model, VertexOutput* out) {
        alignas(16) float px[4];
        alignas(16) float py[4];
        alignas(16) float pz[4];
        alignas(16) float nx[4];
        alignas(16) float ny[4];
        alignas(16) float nz[4];
        alignas(16) float rx[4];
        alignas(16) float ry[4];
        alignas(16) float rz[4];
        alignas(16) float rw[4];

        size_t i = 0;
        for (; i + 4 <= count; i += 4) {
            for (int k = 0; k < 4; ++k) {
                const core::Vertex& v = in[i + static_cast<size_t>(k)];
                px[k] = v.position.x;
                py[k] = v.position.y;
                pz[k] = v.position.z;
                nx[k] = v.normal.x;
                ny[k] = v.normal.y;
                nz[k] = v.normal.z;
            }

            const __m128 X = _mm_load_ps(px);
            const __m128 Y = _mm_load_ps(py);
            const __m128 Z = _mm_load_ps(pz);
            const __m128 one = _mm_set1_ps(1.0f);

            // clip = mvp * position, world = model * position, normal = model * normal
            __m128 cx, cy, cz, cw;
            math::transform4(mvp, X, Y, Z, one, cx, cy, cz, cw);
            __m128 wx, wy, wz, wIgnore;
            math::transform4(model, X, Y, Z, one, wx, wy, wz, wIgnore);
            __m128 mnx, mny, mnz;
            math::transformDirections4(model, _mm_load_ps(nx), _mm_load_ps(ny), _mm_load_ps(nz), mnx,
                                       mny, mnz);

            _mm_store_ps(rx, cx);
            _mm_store_ps(ry, cy);
            _mm_store_ps(rz, cz);
            _mm_store_ps(rw, cw);
            for (int k = 0; k < 4; ++k) {
                VertexOutput& o = out[i + static_cast<size_t>(k)];
                o.clip = {rx[k], ry[k], rz[k], rw[k]};
            }

            _mm_store_ps(rx, wx);
            _mm_store_ps(ry, wy);
            _mm_store_ps(rz, wz);
            for (int k = 0; k < 4; ++k) {
                out[i + static_cast<size_t>(k)].varying.worldPosition = {rx[k], ry[k], rz[k]};
            }

            _mm_store_ps(rx, mnx);
            _mm_store_ps(ry, mny);
            _mm_store_ps(rz, mnz);
            for (int k = 0; k < 4; ++k) {
                VertexOutput& o = out[i + static_cast<size_t>(k)];
                o.varying.normal = {rx[k], ry[k], rz[k]};
                o.varying.color = in[i + static_cast<size_t>(k)].color;
                o.varying.uv = in[i + static_cast<size_t>(k)].uv;
            }
        }

        // Tail vertices take the scalar path with the same matrices.
        for (; i < count; ++i) shadeOne(in[i], mvp, model, out[i]);
    }
#endif
};

}  // namespace nova::shader
