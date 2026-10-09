// NOVA GPU - Software GPU
// src/pipeline/triangle_setup.h - stage 4: screen-space triangle setup.
//
// Converts a clipped triangle into the form the tiler needs:
//   * viewport transform + perspective divide
//   * back-face culling / degeneracy rejection
//   * per-vertex 1/w and attribute/w terms for perspective-correct interpolation
//   * screen-space edge functions ("weights") with a top-left fill rule
//   * a clamped integer bounding box
//
// Depth is interpolated linearly in screen space. That is exact, not an
// approximation: window depth is an affine function of 1/w, and 1/w is itself
// linear in screen space.
#pragma once

#include <algorithm>
#include <cmath>

#include "pipeline/clip_vertex.h"
#include "pipeline/viewport.h"
#include "shader/varying.h"

namespace nova::pipeline {

// Viewport-space triangle plus its precomputed edge equations.
//
// The three weight functions w_i(x, y) = weightA[i] * x + weightB[i] * y +
// weightC[i] are already divided by twice the (positive) triangle area, so they
// are the barycentric coordinates directly: they sum to 1 and the interior is
// exactly the region where all three are >= 0.
struct ScreenTriangle {
    math::Vec2 pos[3];                 // pixel-space vertex positions
    float depth[3] = {};               // window depth in [0, 1]
    float invW[3] = {};                // 1 / clip w
    shader::Varying varyingOverW[3];   // varyings / w (perspective correction)

    float weightA[3] = {};  // barycentric edge coefficients
    float weightB[3] = {};
    float weightC[3] = {};
    bool topLeft[3] = {false, false, false};  // fill rule: accept w_i == 0 here
    float area = 0.0f;      // twice the signed screen area (always > 0 here)

    int minX = 0;  // inclusive, clamped to the viewport
    int minY = 0;
    int maxX = -1;  // inclusive; maxX < minX means "no coverage"
    int maxY = -1;

    bool hasCoverage() const { return maxX >= minX && maxY >= minY; }
};

enum class SetupResult {
    Ok,          // triangle may cover pixels
    Degenerate,  // zero area
    Culled,      // rejected by back-face culling
};

// Fills `out` from a clip-space triangle. `in` must already be clipped so that
// every w > 0.
SetupResult setupTriangle(const ClipVertex in[3], const Viewport& viewport, bool frontFaceCCW,
                          bool backfaceCulling, ScreenTriangle& out);

}  // namespace nova::pipeline
