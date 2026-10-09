// NOVA GPU - Software GPU
// tests/test_shaders.cpp - the Phase 2 shader system.
//
// Covers: the default transform vertex shader, application-supplied vertex
// shaders actually being used by the pipeline, fragment shaders reading
// interpolated attributes (color, uv, screen position, depth), perspective-
// correct uv interpolation, lighting from the uniform block, and per-draw
// uniform capture.
#include "test_framework.h"
#include "test_helpers.h"

#include "core/config.h"
#include "gpu/device.h"
#include "math/math_utils.h"
#include "shader/checker_shader.h"
#include "shader/color_shader.h"
#include "shader/lambert_shader.h"
#include "lighting/light.h"
#include "scene/shapes.h"
#include "shader/transform_vertex_shader.h"

using namespace nova;

namespace {

core::RenderConfig testConfig(int width, int height) {
    core::RenderConfig config;
    config.width = width;
    config.height = height;
    config.tileSize = 8;
    config.enableThreads = false;
    config.backfaceCulling = true;
    return config;
}

// Vertex shader for tests that want to control w directly: position.xy holds the
// desired NDC position (as seen after the perspective divide) and position.z the
// clip-space w. The shader pre-multiplies by w for the divide, exactly like a
// projection matrix would.
class NdcVertexShader final : public shader::VertexShader {
public:
    void shade(const core::Vertex& in, const shader::ShaderUniforms&,
               shader::VertexOutput& out) const override {
        const float w = in.position.z;
        out.clip = {in.position.x * w, in.position.y * w, 0.0f, w};
        out.varying.uv = in.uv;
    }
    const char* name() const override { return "NdcVertexShader"; }
};

// Pass-through vertex shader with an NDC offset, used to prove the pipeline
// really calls the shader the application submitted.
class OffsetVertexShader final : public shader::VertexShader {
public:
    explicit OffsetVertexShader(const math::Vec2& offset) : offset_(offset) {}
    void shade(const core::Vertex& in, const shader::ShaderUniforms&,
               shader::VertexOutput& out) const override {
        out.clip = {in.position.x + offset_.x, in.position.y + offset_.y, 0.0f, 1.0f};
    }
    const char* name() const override { return "OffsetVertexShader"; }

private:
    math::Vec2 offset_;
};

// Writes the interpolated uv into the color channels.
class UvFragmentShader final : public shader::FragmentShader {
public:
    math::Vec4 shade(const shader::FragmentInput& in, const shader::ShaderUniforms&) const override {
        return {in.varying.uv.x, in.varying.uv.y, 0.0f, 1.0f};
    }
    const char* name() const override { return "UvFragmentShader"; }
};

// Writes screen position and depth into the color channels.
class ScreenSpaceFragmentShader final : public shader::FragmentShader {
public:
    explicit ScreenSpaceFragmentShader(float width, float height) : width_(width), height_(height) {}
    math::Vec4 shade(const shader::FragmentInput& in, const shader::ShaderUniforms&) const override {
        return {in.screenPos.x / width_, in.screenPos.y / height_, in.depth, 1.0f};
    }
    const char* name() const override { return "ScreenSpaceFragmentShader"; }

private:
    float width_;
    float height_;
};

// Average x of every non-black pixel, in pixels.
double centroidX(const core::Framebuffer& framebuffer, const core::Color& background) {
    double sum = 0.0;
    size_t count = 0;
    const core::Color* pixels = framebuffer.colorData();
    for (int y = 0; y < framebuffer.height(); ++y) {
        for (int x = 0; x < framebuffer.width(); ++x) {
            const core::Color& c = pixels[static_cast<size_t>(y) * framebuffer.width() + x];
            if (c != background) {
                sum += static_cast<double>(x);
                ++count;
            }
        }
    }
    return count > 0 ? sum / static_cast<double>(count) : 0.0;
}

}  // namespace

NOVA_TEST(Shaders, TransformVertexShaderMapsModelToClipAndWorld) {
    const shader::TransformVertexShader shader;

    shader::ShaderUniforms uniforms;
    uniforms.model = math::translation({1.0f, 2.0f, 3.0f}) * math::rotationZ(math::radians(90.0f));
    uniforms.viewProjection = math::Mat4::identity();

    core::Vertex in;
    in.position = {1.0f, 0.0f, 0.0f};
    in.normal = {1.0f, 0.0f, 0.0f};
    in.color = {0.25f, 0.5f, 0.75f};
    in.uv = {0.125f, 0.875f};

    shader::VertexOutput out;
    shader.shade(in, uniforms, out);

    // rotationZ(90) turns +X into +Y; translation then moves the vertex.
    CHECK_NEAR(out.clip.x, 1.0f, 1e-4);
    CHECK_NEAR(out.clip.y, 3.0f, 1e-4);
    CHECK_NEAR(out.clip.z, 3.0f, 1e-4);
    CHECK_NEAR(out.clip.w, 1.0f, 1e-6);

    CHECK_NEAR(out.varying.worldPosition.x, 1.0f, 1e-4);
    CHECK_NEAR(out.varying.worldPosition.y, 3.0f, 1e-4);
    // The normal is rotated but not translated.
    CHECK_NEAR(out.varying.normal.x, 0.0f, 1e-4);
    CHECK_NEAR(out.varying.normal.y, 1.0f, 1e-4);
    CHECK_NEAR(out.varying.color.z, 0.75f, 1e-6);
    CHECK_NEAR(out.varying.uv.y, 0.875f, 1e-6);
}

NOVA_TEST(Shaders, DefaultVertexShaderMatchesExplicitTransformShader) {
    const scene::Mesh quad = novatest::makeQuad(0.5f, 0.5f, 0.0f, {0.8f, 0.4f, 0.2f});
    const math::Mat4 model = math::translation({0.1f, -0.2f, 0.0f}) * math::rotationY(0.4f);
    const math::Mat4 viewProjection = math::Mat4::identity();
    const shader::ColorFragmentShader fragment;
    const shader::TransformVertexShader explicitVertex;

    gpu::Device withDefault(testConfig(64, 64));
    withDefault.beginFrame(core::kBlack);
    withDefault.draw(quad, model, viewProjection, fragment);
    withDefault.endFrame();

    gpu::Device withExplicit(testConfig(64, 64));
    withExplicit.beginFrame(core::kBlack);
    withExplicit.draw(quad, model, viewProjection, explicitVertex, fragment);
    withExplicit.endFrame();

    size_t mismatches = 0;
    for (size_t i = 0; i < withDefault.framebuffer().pixelCount(); ++i) {
        if (withDefault.framebuffer().colorData()[i] != withExplicit.framebuffer().colorData()[i]) {
            ++mismatches;
        }
    }
    CHECK_EQ(static_cast<int>(mismatches), 0);
    CHECK(novatest::countPixels(withDefault.framebuffer(), core::kBlack) <
          static_cast<int>(withDefault.framebuffer().pixelCount()));
}

NOVA_TEST(Shaders, ApplicationVertexShaderIsUsedByThePipeline) {
    // Same triangle drawn through two different vertex shaders: a plain
    // pass-through and the same shader shifted half a unit in NDC. If the
    // pipeline ignored the shader, both images would be identical.
    const scene::Mesh mesh = novatest::makeTriangle({-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f},
                                                    {0.0f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f});
    const shader::ColorFragmentShader fragment;
    const OffsetVertexShader plain({0.0f, 0.0f});
    const OffsetVertexShader shifted({0.4f, 0.0f});

    gpu::Device plainDevice(testConfig(64, 64));
    plainDevice.beginFrame(core::kBlack);
    plainDevice.draw(mesh, math::Mat4::identity(), math::Mat4::identity(), plain, fragment);
    plainDevice.endFrame();

    gpu::Device shiftedDevice(testConfig(64, 64));
    shiftedDevice.beginFrame(core::kBlack);
    shiftedDevice.draw(mesh, math::Mat4::identity(), math::Mat4::identity(), shifted, fragment);
    shiftedDevice.endFrame();

    const size_t plainPixels = novatest::countPixels(plainDevice.framebuffer(), core::kWhite);
    const size_t shiftedPixels = novatest::countPixels(shiftedDevice.framebuffer(), core::kWhite);
    CHECK(plainPixels > 400);
    CHECK(shiftedPixels > 400);

    // Clipping at the right edge removes a few pixels; the centroid must move by
    // almost exactly the 0.4 NDC = 12.8 px offset.
    const double plainCentroid = centroidX(plainDevice.framebuffer(), core::kBlack);
    const double shiftedCentroid = centroidX(shiftedDevice.framebuffer(), core::kBlack);
    CHECK_NEAR(shiftedCentroid - plainCentroid, 12.8, 0.5);
}

NOVA_TEST(Shaders, FragmentShaderReceivesScreenPositionAndDepth) {
    // A full-screen quad in NDC with the default transform vertex shader.
    gpu::Device device(testConfig(64, 64));
    device.beginFrame(core::kBlack);

    scene::Mesh fullScreen;
    const math::Vec3 corners[4] = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
                                   {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    for (int i = 0; i < 4; ++i) {
        core::Vertex v;
        v.position = corners[i];
        fullScreen.vertices.push_back(v);
    }
    fullScreen.addTriangle(0, 1, 2);
    fullScreen.addTriangle(0, 2, 3);

    const ScreenSpaceFragmentShader fragment(64.0f, 64.0f);
    device.draw(fullScreen, math::Mat4::identity(), math::Mat4::identity(), fragment);
    device.endFrame();

    CHECK_EQ(static_cast<int>(novatest::countPixelsNear(device.framebuffer(), core::kBlack)), 0);

    // Pixel (10, 20) has center (10.5, 20.5), depth 0.5.
    const core::Color px = device.framebuffer().colorData()[20 * 64 + 10];
    CHECK_NEAR(px.r, 10.5f / 64.0f * 255.0f, 1.0f);
    CHECK_NEAR(px.g, 20.5f / 64.0f * 255.0f, 1.0f);
    CHECK_NEAR(px.b, 0.5f * 255.0f, 1.0f);
}

NOVA_TEST(Shaders, PerspectiveCorrectUvInterpolation) {
    // Screen-space triangle A(16.5,48.5) B(48.5,48.5) C(32.5,16.5) with clip w
    // 1, 2, 4. At pixel (32, 24), center (32.5, 24.5), the barycentric weights
    // are wA = 0.125, wB = 0.125, wC = 0.75:
    //   uv = sum(wi * uv_i / w_i) / sum(wi / w_i)
    //   u  = (0.125 * 1/2) / (0.125 + 0.0625 + 0.1875) = 1/6
    //   v  = (0.75 * 1/4)  / 0.375                    = 0.5
    // Plain affine interpolation would give u = 0.125 and v = 0.75, so this
    // test distinguishes correct perspective interpolation from the naive one.
    gpu::Device device(testConfig(64, 64));
    device.beginFrame(core::kBlack);

    scene::Mesh mesh;
    const math::Vec3 positions[3] = {
        {16.5f / 32.0f - 1.0f, 1.0f - 48.5f / 32.0f, 1.0f},  // A: w = 1
        {48.5f / 32.0f - 1.0f, 1.0f - 48.5f / 32.0f, 2.0f},  // B: w = 2
        {32.5f / 32.0f - 1.0f, 1.0f - 16.5f / 32.0f, 4.0f},  // C: w = 4
    };
    const math::Vec2 uvs[3] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
    for (int i = 0; i < 3; ++i) {
        core::Vertex v;
        v.position = positions[i];
        v.uv = uvs[i];
        mesh.vertices.push_back(v);
    }
    mesh.addTriangle(0, 1, 2);

    const NdcVertexShader vertex;
    const UvFragmentShader fragment;
    device.draw(mesh, math::Mat4::identity(), math::Mat4::identity(), vertex, fragment);
    device.endFrame();

    // (32, 24) must be covered by exactly this triangle.
    const core::Color px = device.framebuffer().colorData()[24 * 64 + 32];
    const core::Color expected = core::colorFromVec4({1.0f / 6.0f, 0.5f, 0.0f, 1.0f});
    CHECK(px.r > 38);  // affine would be 0.125 * 255 = 31-32
    CHECK_NEAR(px.r, expected.r, 1.0f);
    CHECK_NEAR(px.g, expected.g, 1.0f);

    // Sanity: the triangle covers a large part of the buffer.
    CHECK(novatest::countPixels(device.framebuffer(), core::kBlack) <
          device.framebuffer().pixelCount());
}

NOVA_TEST(Shaders, LambertLightingUsesInterpolatedNormalsAndUniforms) {
    const scene::Mesh quad = novatest::makeQuad(0.5f, 0.5f, 0.0f, {1.0f, 1.0f, 1.0f});
    const shader::LambertFragmentShader fragment;

    // Light travelling towards -Z, i.e. coming from +Z: the quad's +Z normal
    // faces it exactly, so diffuse is 1 and the surface is fully lit.
    gpu::Device lit(testConfig(64, 64));
    lit.beginFrame(core::kBlack);
    lighting::AmbientLight ambient;
    ambient.color = {1.0f, 1.0f, 1.0f};
    ambient.intensity = 0.15f;
    lit.setAmbient(ambient);

    lighting::DirectionalLight front;
    front.direction = {0.0f, 0.0f, -1.0f};
    front.color = {1.0f, 1.0f, 1.0f};
    lit.setLight(front);
    lit.draw(quad, math::Mat4::identity(), math::Mat4::identity(), fragment);
    lit.endFrame();
    CHECK_EQ(static_cast<int>(novatest::countPixelsNear(lit.framebuffer(), core::kWhite, 1)), 1024);

    // Light travelling towards +Z: the normal points away, diffuse is 0, and
    // only the ambient term remains. Ambient must be part of the result, so the
    // image is a mid gray - not black.
    gpu::Device backlit(testConfig(64, 64));
    backlit.beginFrame(core::kBlack);
    lighting::DirectionalLight behind;
    behind.direction = {0.0f, 0.0f, 1.0f};
    behind.color = {1.0f, 1.0f, 1.0f};
    backlit.setAmbient(ambient);
    backlit.setLight(behind);
    backlit.draw(quad, math::Mat4::identity(), math::Mat4::identity(), fragment);
    backlit.endFrame();

    const core::Color ambientOnly = core::colorFromVec4({0.15f, 0.15f, 0.15f, 1.0f});
    CHECK_EQ(static_cast<int>(novatest::countPixelsNear(backlit.framebuffer(), ambientOnly, 1)),
             1024);
    CHECK(ambientOnly.r > 0);
}

NOVA_TEST(Shaders, CheckerShaderUsesInterpolatedTextureCoordinates) {
    // scene::makeQuad assigns uv (0,0)..(1,1); the test helper's quad leaves uv
    // at the origin, which would make every fragment land in the same cell.
    const scene::Mesh quad = scene::makeQuad(1.0f, 1.0f, {1.0f, 1.0f, 1.0f});
    const shader::CheckerFragmentShader fragment(2.0f);  // 2x2 cells over uv 0..1

    gpu::Device device(testConfig(64, 64));
    device.beginFrame(core::kBlack);
    device.draw(quad, math::Mat4::identity(), math::Mat4::identity(), fragment);
    device.endFrame();

    // The quad covers 32x32 pixels; each of the four checker cells covers a
    // 16x16 quarter, and the two cell colors must both appear.
    const core::Color light = core::colorFromVec4({0.92f, 0.92f, 0.95f, 1.0f});
    const core::Color dark = core::colorFromVec4({0.16f, 0.16f, 0.22f, 1.0f});
    const size_t lightPixels = novatest::countPixelsNear(device.framebuffer(), light, 1);
    const size_t darkPixels = novatest::countPixelsNear(device.framebuffer(), dark, 1);
    CHECK(lightPixels > 400);
    CHECK(darkPixels > 400);
    CHECK_EQ(static_cast<int>(lightPixels + darkPixels), 1024);

    // scene::makeQuad assigns uv (0,0) to the bottom-left corner in NDC, i.e.
    // uv.y = 0 is the *bottom* of the image and increases upwards while screen y
    // increases downwards. For pixels 16..47 the quad uv is
    //   u = (x + 0.5 - 16) / 32,  v = (48 - (y + 0.5)) / 32
    // so with 2 cells the quadrant pattern is: top-left (0,1) dark,
    // top-right (1,1) light, bottom-left (0,0) light, bottom-right (1,0) dark.
    const core::Framebuffer& fb = device.framebuffer();
    CHECK(fb.colorData()[24 * 64 + 24] == dark);   // u 0.27, v 0.73 -> cell (0,1)
    CHECK(fb.colorData()[24 * 64 + 40] == light);  // u 0.77, v 0.73 -> cell (1,1)
    CHECK(fb.colorData()[40 * 64 + 24] == light);  // u 0.27, v 0.23 -> cell (0,0)
    CHECK(fb.colorData()[40 * 64 + 40] == dark);   // u 0.77, v 0.23 -> cell (1,0)
}

NOVA_TEST(Shaders, UniformsAreCapturedWhenTheCommandIsRecorded) {
    // Draw with a "front" light, then change the device light before endFrame().
    // The recorded draw must still use the front light: commands capture their
    // uniforms at record time, exactly like a constant buffer.
    const scene::Mesh quad = novatest::makeQuad(0.5f, 0.5f, 0.0f, {1.0f, 1.0f, 1.0f});
    const shader::LambertFragmentShader fragment;

    gpu::Device device(testConfig(64, 64));
    device.beginFrame(core::kBlack);

    lighting::AmbientLight ambient;
    ambient.intensity = 0.0f;
    device.setAmbient(ambient);

    lighting::DirectionalLight front;
    front.direction = {0.0f, 0.0f, -1.0f};
    device.setLight(front);
    device.draw(quad, math::Mat4::identity(), math::Mat4::identity(), fragment);

    lighting::DirectionalLight behind;
    behind.direction = {0.0f, 0.0f, 1.0f};  // would render ambient-only (black here)
    device.setLight(behind);

    device.endFrame();
    CHECK_EQ(static_cast<int>(novatest::countPixelsNear(device.framebuffer(), core::kWhite, 1)), 1024);
}

NOVA_TEST(Shaders, ShaderNamesAreReported) {
    const shader::TransformVertexShader vertex;
    const shader::ColorFragmentShader color;
    const shader::LambertFragmentShader lambert;
    const shader::CheckerFragmentShader checker;
    CHECK(std::string(vertex.name()) == "TransformVertexShader");
    CHECK(std::string(color.name()) == "ColorFragmentShader");
    CHECK(std::string(lambert.name()) == "LambertFragmentShader");
    CHECK(std::string(checker.name()) == "CheckerFragmentShader");
}
