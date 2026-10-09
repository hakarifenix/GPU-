// NOVA GPU - Software GPU
// src/scene/mesh.h - indexed triangle mesh (vertex + index buffers).
#pragma once

#include <cstddef>
#include <cstdint>

#include "core/vertex.h"
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
};

}  // namespace nova::scene
