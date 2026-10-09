// NOVA GPU - Software GPU
// src/core/vertex.h - the vertex format consumed by the pipeline.
#pragma once

#include "math/vec.h"

namespace nova::core {

// Standard NOVA vertex format. Phase 1 only reads position and color; normal
// and uv are carried through the pipeline already so that Phase 2 (shaders),
// Phase 3 (textures) and Phase 4 (lighting) need no format change.
struct Vertex {
    math::Vec3 position;
    math::Vec3 normal{0.0f, 0.0f, 1.0f};
    math::Vec3 color{1.0f, 1.0f, 1.0f};
    math::Vec2 uv{0.0f, 0.0f};
};

}  // namespace nova::core
