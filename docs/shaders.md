# NOVA GPU shaders (Phase 2)

NOVA has no GLSL compiler and no runtime shader compilation on purpose. A shader
is a small C++ class, and the pipeline talks to it through two interfaces:

```
mesh vertex (model space)                       per draw
        |                                          |
        v                                          v
  VertexShader::shade(vertex, uniforms, out)   ShaderUniforms
        |   writes clip position + varyings        |
        v                                          |
   clipping  (interpolates clip + varyings)        |
        |                                          |
        v                                          |
  triangle setup  (varyings / w)                   |
        |                                          |
        v                                          |
  rasterizer  (perspective-correct interpolation)  |
        |                                          |
        v                                          v
  FragmentShader::shade(fragmentInput, uniforms) -> color
```

## Files

| File | What it defines |
| --- | --- |
| `src/shader/uniforms.h` | `ShaderUniforms` (matrices, camera, light, time) + `DirectionalLight` |
| `src/shader/varying.h` | `Varying` (world position, normal, color, uv) + interpolation helpers |
| `src/shader/vertex_shader.h` | `VertexOutput`, `VertexShader` |
| `src/shader/fragment_shader.h` | `FragmentInput`, `FragmentShader` |
| `src/shader/transform_vertex_shader.h` | default vertex shader: object -> clip, fills all varyings |
| `src/shader/color_shader.h` | unlit interpolated vertex color (the Phase 1 look) |
| `src/shader/lambert_shader.h` | ambient + directional diffuse from the interpolated normal |
| `src/shader/checker_shader.h` | procedural checkerboard from the interpolated uv |

## The inter-stage payload

`shader::Varying` is the whole "shader language" of Phase 2 - a plain struct:

```cpp
struct Varying {
    math::Vec3 worldPosition;
    math::Vec3 normal;
    math::Vec3 color;
    math::Vec2 uv;
};
```

Add a field to extend it, then:

* `lerpVarying()` (used by clipping) and `operator+` / `operator*` (used by
  perspective-correct interpolation) are the only places that must learn about
  the new field, plus the vertex shader that fills it.
* The clipper, triangle setup and rasterizer need no changes.

## Writing a shader

```cpp
class WaveVertexShader final : public shader::VertexShader {
public:
    void shade(const nova::core::Vertex& in, const shader::ShaderUniforms& u,
               shader::VertexOutput& out) const override {
        const math::Vec3 displaced =
            in.position + in.normal * std::sin(u.time + in.position.x) * 0.1f;
        out.clip = math::transform(u.viewProjection * u.model, math::Vec4{displaced, 1.0f});
        out.varying.worldPosition = math::transformPoint(u.model, displaced);
        out.varying.normal = math::transformDirection(u.model, in.normal);
        out.varying.color = in.color;
        out.varying.uv = in.uv;
    }
    const char* name() const override { return "WaveVertexShader"; }
};

class SolidFragmentShader final : public shader::FragmentShader {
public:
    math::Vec4 shade(const shader::FragmentInput& in, const shader::ShaderUniforms&) const override {
        return {0.2f, 0.6f, 1.0f, 1.0f};
    }
};
```

and draw with:

```cpp
device.setCameraPosition(cam.position());
device.setLight(light);
device.draw(mesh, model, cam.viewProjection(), waveVertex, solidFragment);
```

`VertexShader::shade` writes into a caller-owned `VertexOutput`, so the per-vertex
loop allocates nothing.

## What the stages guarantee

* Varyings are interpolated **perspective-correctly** for every fragment:
  `attribute / w` is interpolated linearly in screen space and divided by the
  interpolated `1 / w`. `Shaders.PerspectiveCorrectUvInterpolation` pins this with
  hand-computed numbers (`u = 1/6`, `v = 0.5` at a known pixel, where affine
  interpolation would give `0.125`, `0.75`).
* Clipping interpolates the clip position *and* the varyings, so a clipped
  triangle keeps correct colors, uvs and normals.
* `FragmentInput::screenPos` is the pixel centre; `FragmentInput::depth` is the
  window depth in `[0, 1]` for the pixel being shaded.
* Uniforms are captured when the draw command is recorded and uploaded once per
  draw (`Rasterizer::addUniformBlock`), so a frame always renders with the
  uniforms it was recorded with - even if device state changes before
  `endFrame()`.
* Coverage, culling, the top-left fill rule and the depth test are unchanged from
  Phase 1; a shader can only affect what a covered, depth-passing fragment looks
  like.

## Device API

```cpp
void setCameraPosition(const math::Vec3& position);
void setLight(const shader::DirectionalLight& light);
void setTime(float seconds);
shader::ShaderUniforms makeUniforms(const math::Mat4& model, const math::Mat4& viewProjection) const;

void draw(mesh, model, viewProjection, fragmentShader);                    // default vertex shader
void draw(mesh, model, viewProjection, vertexShader, fragmentShader);      // explicit vertex shader
```

## Demo

```bash
./build/bin/nova_shader_demo --width 640 --height 480
```

writes `shader_color.ppm`, `shader_lambert.ppm`, `shader_checker.ppm` and
`shader_wave.ppm` from one scene - only the shader objects differ between them.

## Status

Phase 2 covers the shader **interface**, the built-in shaders and the uniform
block. Phase 3 adds real texture sampling next to `CheckerFragmentShader`
(nearest/bilinear filtering, mipmaps), and Phase 4 turns `LambertFragmentShader`
into a full CPU-friendly lighting module (several lights, specular).
