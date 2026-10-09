// NOVA GPU - Software GPU
// src/scene/shapes.h - procedural meshes used by the demos and benchmarks.
#pragma once

#include <array>
#include <cmath>

#include "scene/mesh.h"

namespace nova::scene {

// Unit cube centred on the origin, 1x1x1 by default.
//
// 24 vertices (4 per face) rather than 8 so that every face has a correct
// outward normal and its own flat color - required for Phase 4 lighting.
inline Mesh makeCube(float size = 1.0f,
                     const std::array<math::Vec3, 6>& faceColors = {
                         math::Vec3{0.90f, 0.25f, 0.25f},  // +X red
                         math::Vec3{0.25f, 0.85f, 0.35f},  // -X green
                         math::Vec3{0.30f, 0.55f, 0.95f},  // +Y blue
                         math::Vec3{0.95f, 0.85f, 0.30f},  // -Y yellow
                         math::Vec3{0.85f, 0.40f, 0.85f},  // +Z magenta
                         math::Vec3{0.35f, 0.85f, 0.85f},  // -Z cyan
                     }) {
    const float h = size * 0.5f;

    // Face order: +X, -X, +Y, -Y, +Z, -Z. Each face lists its 4 corners in
    // counter-clockwise order as seen from outside the cube.
    struct Face {
        math::Vec3 normal;
        math::Vec3 c[4];
    };
    const Face faces[6] = {
        {{ 1, 0, 0}, {{ h, h,-h}, { h, h, h}, { h,-h, h}, { h,-h,-h}}},
        {{-1, 0, 0}, {{-h, h, h}, {-h, h,-h}, {-h,-h,-h}, {-h,-h, h}}},
        {{ 0, 1, 0}, {{-h, h, h}, { h, h, h}, { h, h,-h}, {-h, h,-h}}},
        {{ 0,-1, 0}, {{-h,-h,-h}, { h,-h,-h}, { h,-h, h}, {-h,-h, h}}},
        {{ 0, 0, 1}, {{-h,-h, h}, { h,-h, h}, { h, h, h}, {-h, h, h}}},
        {{ 0, 0,-1}, {{ h,-h,-h}, {-h,-h,-h}, {-h, h,-h}, { h, h,-h}}},
    };

    const float u[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    Mesh mesh;
    mesh.vertices.reserve(24);
    mesh.indices.reserve(36);

    for (size_t f = 0; f < 6; ++f) {
        const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        for (int i = 0; i < 4; ++i) {
            core::Vertex v;
            v.position = faces[f].c[i];
            v.normal = faces[f].normal;
            v.color = faceColors[f];
            v.uv = {u[i][0], u[i][1]};
            mesh.vertices.push_back(v);
        }
        mesh.addTriangle(base + 0, base + 1, base + 2);
        mesh.addTriangle(base + 0, base + 2, base + 3);
    }
    return mesh;
}

// UV sphere centred on the origin with smooth (analytically exact) normals.
// Lighting models show up far better on a curved surface than on a cube: the
// diffuse falloff and the specular highlight become visible gradients.
inline Mesh makeSphere(float radius = 1.0f, int slices = 32, int stacks = 16) {
    if (slices < 3) slices = 3;
    if (stacks < 2) stacks = 2;

    Mesh mesh;
    mesh.vertices.reserve(static_cast<size_t>((stacks + 1) * (slices + 1)));
    mesh.indices.reserve(static_cast<size_t>(stacks * slices * 6));

    constexpr float kPi = 3.14159265358979323846f;
    for (int stack = 0; stack <= stacks; ++stack) {
        const float v = static_cast<float>(stack) / static_cast<float>(stacks);
        const float phi = v * kPi;                 // 0 at the north pole
        const float y = std::cos(phi);
        const float ringRadius = std::sin(phi);
        for (int slice = 0; slice <= slices; ++slice) {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float theta = u * 2.0f * kPi;
            const math::Vec3 unit{ringRadius * std::sin(theta), y, ringRadius * std::cos(theta)};

            core::Vertex vertex;
            vertex.position = unit * radius;
            vertex.normal = unit;  // exact: no per-triangle normal averaging
            // A slight checker tint around the sphere makes the uv layout visible
            // without a texture.
            const int cellU = static_cast<int>(u * 8.0f);
            const int cellV = static_cast<int>(v * 4.0f);
            vertex.color = ((cellU + cellV) & 1) ? math::Vec3{0.85f, 0.85f, 0.9f}
                                                  : math::Vec3{0.75f, 0.78f, 0.85f};
            vertex.uv = {u, 1.0f - v};
            mesh.vertices.push_back(vertex);
        }
    }

    const uint32_t rowStride = static_cast<uint32_t>(slices + 1);
    for (int stack = 0; stack < stacks; ++stack) {
        for (int slice = 0; slice < slices; ++slice) {
            const uint32_t a = static_cast<uint32_t>(stack) * rowStride + static_cast<uint32_t>(slice);
            const uint32_t b = a + rowStride;
            // Wound counter-clockwise as seen from outside the sphere. At the
            // poles the two vertices of the top (or bottom) row coincide, so the
            // quad collapses to a single triangle there - emitting both would
            // add a zero-area triangle that culling then has to discard.
            if (stack != 0) mesh.addTriangle(a, b, a + 1);
            if (stack != stacks - 1) mesh.addTriangle(a + 1, b, b + 1);
        }
    }
    return mesh;
}

// Flat quad in the XY plane facing +Z, centered on the origin.
inline Mesh makeQuad(float width = 1.0f, float height = 1.0f,
                     const math::Vec3& color = {1.0f, 1.0f, 1.0f}) {
    const float hw = width * 0.5f;
    const float hh = height * 0.5f;
    Mesh mesh;
    const math::Vec3 corners[4] = {{-hw, -hh, 0.0f}, {hw, -hh, 0.0f}, {hw, hh, 0.0f}, {-hw, hh, 0.0f}};
    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        core::Vertex v;
        v.position = corners[i];
        v.normal = {0.0f, 0.0f, 1.0f};
        v.color = color;
        v.uv = {uv[i][0], uv[i][1]};
        mesh.vertices.push_back(v);
    }
    mesh.addTriangle(0, 1, 2);
    mesh.addTriangle(0, 2, 3);
    return mesh;
}

}  // namespace nova::scene
