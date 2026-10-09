// NOVA GPU - Software GPU
// src/pipeline/viewport.h - NDC -> window (screen pixel) transform.
#pragma once

#include "math/vec.h"

namespace nova::pipeline {

// Maps normalized device coordinates (x,y in [-1, 1], z in [-1, 1]) onto the
// framebuffer. NDC y points up, screen y points down, hence the flip.
struct Viewport {
    float x = 0.0f;       // left edge in pixels
    float y = 0.0f;       // top edge in pixels
    float width = 0.0f;   // pixels
    float height = 0.0f;  // pixels
    float minDepth = 0.0f;
    float maxDepth = 1.0f;

    static Viewport forFramebuffer(int width, int height) {
        Viewport vp;
        vp.width = static_cast<float>(width);
        vp.height = static_cast<float>(height);
        return vp;
    }

    math::Vec2 ndcToScreen(const math::Vec2& ndc) const {
        return {x + (ndc.x * 0.5f + 0.5f) * width, y + (0.5f - ndc.y * 0.5f) * height};
    }

    // NDC depth [-1, 1] -> window depth [0, 1].
    float ndcToDepth(float ndcZ) const {
        return minDepth + math::clampf(ndcZ * 0.5f + 0.5f, 0.0f, 1.0f) * (maxDepth - minDepth);
    }
};

}  // namespace nova::pipeline
