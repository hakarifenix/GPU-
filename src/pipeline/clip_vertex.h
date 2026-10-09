// NOVA GPU - Software GPU
// src/pipeline/clip_vertex.h - the vertex payload that flows through the
// vertex processing -> clipping -> triangle setup stages.
#pragma once

#include "math/vec.h"
#include "shader/varying.h"
#include "shader/vertex_shader.h"

namespace nova::pipeline {

// A vertex in homogeneous clip space plus the shader varyings that must survive
// clipping. Clipping interpolates all of these linearly in clip space, which is
// correct because clip space is linear in the triangle's parameter domain.
// Deriving from VertexOutput (rather than holding one) lets a shader write its
// batch directly into the pipeline's vertex array - same layout, no copy.
struct ClipVertex : shader::VertexOutput {
    // clip (math::Vec4) and varying (shader::Varying) come from VertexOutput.

    float invW() const { return clip.w != 0.0f ? 1.0f / clip.w : 0.0f; }
};

// Linear interpolation between two clip vertices; t = 0 gives `a`, t = 1 `b`.
inline ClipVertex lerpClipVertex(const ClipVertex& a, const ClipVertex& b, float t) {
    ClipVertex out;
    out.clip = math::lerp(a.clip, b.clip, t);
    out.varying = shader::lerpVarying(a.varying, b.varying, t);
    return out;
}

}  // namespace nova::pipeline
