// NOVA GPU - Software GPU
// src/scene/mesh.h - indexed triangle mesh (vertex + index buffers).
#pragma once

#include <cstddef>
#include <cstdint>

#include "core/vertex.h"
#include "math/bounds.h"
#include "memory/buffer.h"

namespace nova::scene {

// Phase 1 supports one primitive type: triangle lists.
struct Mesh {
    memory::Buffer<core::Vertex> vertices;
    memory::Buffer<uint32_t> indices;  // 3 indices per triangle

    size_t vertexCount() const { return vertices.size(); }
    size_t indexCount() const { return indices.size(); }
    size_t triangleCount() const { return indices.size() / 3; }

    bool empty() const { return vertices.empty() || indices.size() < 3; }

    void addTriangle(uint32_t a, uint32_t b, uint32_t c) {
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }

    size_t memoryBytes() const { return vertices.bytes() + indices.bytes(); }

    // Object-space bounds, computed once on first use and cached (the visibility
    // stage asks for them every frame, and geometry is uploaded once).
    const math::Aabb& bounds() const {
        if (!boundsValid_) {
            math::Aabb box;
            for (size_t i = 0; i < vertices.size(); ++i) box.expand(vertices[i].position);
            cachedBounds_ = box;
            boundsValid_ = true;
        }
        return cachedBounds_;
    }

    // Bounding sphere around the bounds centre; used for cheap object tests.
    math::BoundingSphere boundingSphere() const {
        if (empty()) return {};
        const math::Aabb& box = bounds();
        return math::BoundingSphere{box.center(), box.radius()};
    }

    // Drops the cached bounds after vertices change.
    void invalidateBounds() { boundsValid_ = false; }

private:
    mutable math::Aabb cachedBounds_;
    mutable bool boundsValid_ = false;
};

}  // namespace nova::scene
