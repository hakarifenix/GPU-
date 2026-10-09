// NOVA GPU - Software GPU
// tests/test_scene.cpp - procedural geometry invariants.
//
// Back-face culling in NOVA is winding based (config.frontFaceCCW), so every
// triangle's winding must agree with the vertex normal it reports. A face wound
// the other way is silently culled when seen from outside, which shows up as a
// hole in the mesh - hard to notice on a cube, easy to catch here.
#include "test_framework.h"

#include <cstdint>

#include "scene/mesh.h"
#include "scene/shapes.h"

using namespace nova;

namespace {

// Returns the number of triangles whose geometric normal (right-hand rule from
// the winding) points against the stored vertex normal.
size_t countInvertedTriangles(const scene::Mesh& mesh) {
    size_t inverted = 0;
    for (size_t t = 0; t < mesh.triangleCount(); ++t) {
        const core::Vertex& v0 = mesh.vertices[mesh.indices[t * 3 + 0]];
        const core::Vertex& v1 = mesh.vertices[mesh.indices[t * 3 + 1]];
        const core::Vertex& v2 = mesh.vertices[mesh.indices[t * 3 + 2]];
        const math::Vec3 geometric = math::cross(v1.position - v0.position, v2.position - v0.position);
        if (math::dot(geometric, v0.normal) <= 0.0f) ++inverted;
    }
    return inverted;
}

}  // namespace

NOVA_TEST(Scene, CubeFaceWindingMatchesNormals) {
    const scene::Mesh cube = scene::makeCube(1.6f);
    CHECK_EQ(static_cast<int>(cube.vertexCount()), 24);
    CHECK_EQ(static_cast<int>(cube.triangleCount()), 12);
    CHECK_EQ(static_cast<int>(countInvertedTriangles(cube)), 0);

    // Every face must report an outward normal: the average of each triangle's
    // vertices points away from the cube centre.
    for (size_t t = 0; t < cube.triangleCount(); ++t) {
        const math::Vec3 center =
            (cube.vertices[cube.indices[t * 3 + 0]].position +
             cube.vertices[cube.indices[t * 3 + 1]].position +
             cube.vertices[cube.indices[t * 3 + 2]].position) /
            3.0f;
        CHECK(math::dot(center, cube.vertices[cube.indices[t * 3 + 0]].normal) > 0.0f);
    }
}

NOVA_TEST(Scene, QuadFaceWindingMatchesNormals) {
    const scene::Mesh quad = scene::makeQuad(1.0f, 1.0f);
    CHECK_EQ(static_cast<int>(quad.triangleCount()), 2);
    CHECK_EQ(static_cast<int>(countInvertedTriangles(quad)), 0);
}

NOVA_TEST(Scene, SphereFaceWindingMatchesNormals) {
    // Smooth-normal sphere used by the lighting demos: every triangle must be
    // wound so that it is front-facing when seen from outside.
    const scene::Mesh sphere = scene::makeSphere(1.0f, 24, 12);
    // stacks * slices * 2 quads, minus one triangle per slice at each pole where
    // the quad collapses into a single triangle.
    CHECK_EQ(static_cast<int>(sphere.triangleCount()), 24 * 12 * 2 - 2 * 24);
    CHECK_EQ(static_cast<int>(countInvertedTriangles(sphere)), 0);

    // Normals are unit length and point outwards.
    for (size_t i = 0; i < sphere.vertexCount(); ++i) {
        const core::Vertex& v = sphere.vertices[i];
        CHECK_NEAR(math::length(v.normal), 1.0f, 1e-4);
        CHECK(math::dot(v.normal, v.position) > 0.0f);
    }
}

NOVA_TEST(Scene, CubeIsIndexedAndContiguous) {
    const scene::Mesh cube = scene::makeCube(1.0f);
    for (size_t i = 0; i < cube.indexCount(); ++i) {
        CHECK(cube.indices[i] < cube.vertexCount());
    }
    CHECK_EQ(static_cast<int>(cube.indexCount()), 36);
    CHECK(cube.memoryBytes() > 0);
}
