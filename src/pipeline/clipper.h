// NOVA GPU - Software GPU
// src/pipeline/clipper.h - stage 3: clip space polygon clipping.
//
// Clipping happens in homogeneous clip space *before* the perspective divide so
// that triangles crossing the camera plane cannot produce infinite screen
// coordinates. Phase 1 clips against two planes:
//
//   * the near plane:  z + w >= 0   (OpenGL NDC convention, z_ndc = z/w >= -1)
//   * the w plane:     w     >= 0   (keeps the perspective divide well defined)
//
// Left/right/top/bottom and far-plane clipping can be added by calling
// clipPolygonAgainstPlane with the corresponding plane; the framebuffer already
// clips those edges during rasterization.
#pragma once

#include "math/vec.h"
#include "pipeline/clip_vertex.h"

namespace nova::pipeline {

// Sutherland-Hodgman clip of a convex polygon against the half-space
// dot(plane, clip) >= 0. `in`/`out` may alias only if out has room beyond the
// input's live range (the implementation reads ahead of the write index).
// Returns the number of vertices written to `out` (0 when fully clipped).
int clipPolygonAgainstPlane(const ClipVertex* in, int inCount, const math::Vec4& plane,
                            ClipVertex* out, int maxOut);

// Clips one triangle against the near and w planes.
//   outCount == 0 -> fully outside (discard)
//   outCount == 3 -> unchanged triangle
//   outCount == 4 -> a quad (re-emit as two triangles: 0,1,2 and 0,2,3)
// enableNearClip == false keeps the w >= 0 plane test only.
int clipTriangle(const ClipVertex in[3], bool enableNearClip, ClipVertex out[4]);

// Plane definitions in clip space.
inline math::Vec4 nearPlane() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
inline math::Vec4 positiveWPlane() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

}  // namespace nova::pipeline
