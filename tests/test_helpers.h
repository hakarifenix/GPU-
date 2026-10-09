// NOVA GPU - Software GPU
// tests/test_helpers.h - shared helpers for renderer tests.
#pragma once

#include "core/framebuffer.h"
#include "scene/mesh.h"

namespace novatest {

// Triangle mesh with three positions and one flat color.
inline nova::scene::Mesh makeTriangle(const nova::math::Vec3& p0, const nova::math::Vec3& p1,
                                      const nova::math::Vec3& p2,
                                      const nova::math::Vec3& color = {1.0f, 1.0f, 1.0f}) {
    nova::scene::Mesh mesh;
    const nova::math::Vec3 positions[3] = {p0, p1, p2};
    for (int i = 0; i < 3; ++i) {
        nova::core::Vertex v;
        v.position = positions[i];
        v.normal = {0.0f, 0.0f, 1.0f};
        v.color = color;
        v.uv = {0.0f, 0.0f};
        mesh.vertices.push_back(v);
    }
    mesh.addTriangle(0, 1, 2);
    return mesh;
}

// Quad (two triangles) as an indexed mesh, CCW when seen from +Z.
inline nova::scene::Mesh makeQuad(float halfWidth, float halfHeight, float z,
                                 const nova::math::Vec3& color = {1.0f, 1.0f, 1.0f}) {
    nova::scene::Mesh mesh;
    const nova::math::Vec3 corners[4] = {{-halfWidth, -halfHeight, z},
                                         {halfWidth, -halfHeight, z},
                                         {halfWidth, halfHeight, z},
                                         {-halfWidth, halfHeight, z}};
    for (int i = 0; i < 4; ++i) {
        nova::core::Vertex v;
        v.position = corners[i];
        v.normal = {0.0f, 0.0f, 1.0f};
        v.color = color;
        v.uv = {0.0f, 0.0f};
        mesh.vertices.push_back(v);
    }
    mesh.addTriangle(0, 1, 2);
    mesh.addTriangle(0, 2, 3);
    return mesh;
}

// Number of pixels equal to `color`.
inline size_t countPixels(const nova::core::Framebuffer& framebuffer, const nova::core::Color& color) {
    size_t count = 0;
    const nova::core::Color* pixels = framebuffer.colorData();
    for (size_t i = 0; i < framebuffer.pixelCount(); ++i) {
        if (pixels[i] == color) ++count;
    }
    return count;
}

// Number of pixels within `tolerance` per channel of `color`.
//
// Exact equality is the wrong check for interpolated colors: perspective-correct
// interpolation of a constant attribute can land 1 LSB away from the exact byte
// when the value falls on a rounding boundary (e.g. 0.1 * 255 + 0.5 == 26.0),
// which real GPUs exhibit as well.
inline size_t countPixelsNear(const nova::core::Framebuffer& framebuffer,
                              const nova::core::Color& color, int tolerance = 1) {
    size_t count = 0;
    const nova::core::Color* pixels = framebuffer.colorData();
    for (size_t i = 0; i < framebuffer.pixelCount(); ++i) {
        const int dr = static_cast<int>(pixels[i].r) - static_cast<int>(color.r);
        const int dg = static_cast<int>(pixels[i].g) - static_cast<int>(color.g);
        const int db = static_cast<int>(pixels[i].b) - static_cast<int>(color.b);
        if (dr <= tolerance && dr >= -tolerance && dg <= tolerance && dg >= -tolerance &&
            db <= tolerance && db >= -tolerance) {
            ++count;
        }
    }
    return count;
}

// True when every pixel is exactly `color`.
inline bool allPixelsAre(const nova::core::Framebuffer& framebuffer, const nova::core::Color& color) {
    return countPixels(framebuffer, color) == framebuffer.pixelCount();
}

}  // namespace novatest
