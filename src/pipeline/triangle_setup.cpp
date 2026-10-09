#include "pipeline/triangle_setup.h"

#include "math/math_utils.h"

namespace nova::pipeline {
namespace {

// Below this, twice-the-area is treated as exactly zero. Screen areas are in
// pixels^2, so this is far below one pixel of coverage.
constexpr float kDegenerateEpsilon = 1e-12f;

struct EdgeCoefficients {
    float a;
    float b;
    float c;
};

// Coefficients of E(from, to, p) = a * p.x + b * p.y + c, where E is the
// standard edge function (twice the signed area of from -> to -> p).
EdgeCoefficients edgeCoefficients(const math::Vec2& from, const math::Vec2& to) {
    EdgeCoefficients e;
    e.a = from.y - to.y;
    e.b = to.x - from.x;
    e.c = -(e.a * from.x + e.b * from.y);
    return e;
}

}  // namespace

SetupResult setupTriangle(const ClipVertex in[3], const Viewport& viewport, bool frontFaceCCW,
                          bool backfaceCulling, ScreenTriangle& out) {
    // 1. Perspective divide + viewport transform.
    for (int i = 0; i < 3; ++i) {
        const ClipVertex& v = in[i];
        const float invW = v.invW();

        out.pos[i] = viewport.ndcToScreen({v.clip.x * invW, v.clip.y * invW});
        out.depth[i] = viewport.ndcToDepth(v.clip.z * invW);
        out.invW[i] = invW;
        // Attributes are pre-multiplied by 1/w so the rasterizer can interpolate
        // them affinely and divide once by the interpolated 1/w (perspective
        // correction). Any new Varying field is handled automatically.
        out.varyingOverW[i] = v.varying * invW;
    }

    // 2. Screen-space orientation. Screen y points down, so a front face that
    //    is counter-clockwise in NDC comes out clockwise (negative area) here.
    const float twiceArea = math::edgeFunction(out.pos[0], out.pos[1], out.pos[2]);
    out.area = twiceArea;
    if (std::fabs(twiceArea) < kDegenerateEpsilon) return SetupResult::Degenerate;

    const bool frontFacing = frontFaceCCW ? (twiceArea < 0.0f) : (twiceArea > 0.0f);
    if (backfaceCulling && !frontFacing) return SetupResult::Culled;

    // 3. Fold the 1/area factor into the edge coefficients so the stored values
    //    are the barycentric weights themselves. The sign of the area is kept:
    //    E_i has the same sign as twiceArea for interior points, so E_i / area is
    //    positive inside for either winding.
    const float scale = 1.0f / twiceArea;

    struct VertexPair {
        int from;
        int to;
    };
    // weight 0 belongs to vertex 0, which is opposite the edge p1 -> p2.
    constexpr VertexPair kEdges[3] = {{1, 2}, {2, 0}, {0, 1}};

    for (int i = 0; i < 3; ++i) {
        const EdgeCoefficients e = edgeCoefficients(out.pos[kEdges[i].from], out.pos[kEdges[i].to]);
        out.weightA[i] = e.a * scale;
        out.weightB[i] = e.b * scale;
        out.weightC[i] = e.c * scale;

        // Top-left fill rule: a boundary pixel belongs to the triangle only if
        // its edge is a top or left edge, so shared edges are drawn exactly once.
        const math::Vec2 d = out.pos[kEdges[i].to] - out.pos[kEdges[i].from];
        const bool edgeIsTopLeft = (d.y < 0.0f) || (d.y == 0.0f && d.x > 0.0f);
        out.topLeft[i] = edgeIsTopLeft;
    }
    out.area = std::fabs(twiceArea);

    // 4. Integer bounding box of the pixel *centers* that can be covered. A pixel
    //    px covers the point px + 0.5, so px ranges over
    //    [ceil(min - 0.5), floor(max - 0.5)], clamped to the framebuffer.
    float minX = std::min({out.pos[0].x, out.pos[1].x, out.pos[2].x});
    float maxX = std::max({out.pos[0].x, out.pos[1].x, out.pos[2].x});
    float minY = std::min({out.pos[0].y, out.pos[1].y, out.pos[2].y});
    float maxY = std::max({out.pos[0].y, out.pos[1].y, out.pos[2].y});

    const int fbW = static_cast<int>(viewport.width);
    const int fbH = static_cast<int>(viewport.height);
    out.minX = std::max(0, static_cast<int>(std::ceil(minX - 0.5f)));
    out.maxX = std::min(fbW - 1, static_cast<int>(std::floor(maxX - 0.5f)));
    out.minY = std::max(0, static_cast<int>(std::ceil(minY - 0.5f)));
    out.maxY = std::min(fbH - 1, static_cast<int>(std::floor(maxY - 0.5f)));

    return SetupResult::Ok;
}

}  // namespace nova::pipeline
