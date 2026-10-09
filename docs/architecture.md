# NOVA GPU architecture

```
application
    |  records commands (mesh, model matrix, view-projection, shader)
    v
NOVA GPU API          src/gpu/device.h
    |
    v
command processor     src/gpu/command_processor.cpp
    |  per draw: vertex processing -> clipping -> triangle setup -> submit
    v
vertex processing     src/pipeline/vertex_processor.cpp
    |  runs the draw's VertexShader: model -> clip (homogeneous, w > 0)
    |                       + varyings (world position, normal, color, uv)
    v
clipping              src/pipeline/clipper.cpp
    |  Sutherland-Hodgman against z + w >= 0 and w >= 0, in clip space
    v
triangle setup        src/pipeline/triangle_setup.cpp
    |  perspective divide, viewport transform, culling, edge equations, bbox
    v
software rasterizer   src/rasterizer/rasterizer.cpp
    |  tile binning, then one tile per parallel job
    v
fragment processing   src/shader/fragment_shader.h
    |  the draw's FragmentShader: FragmentInput (+ uniforms) -> color
    |  textures: src/texture/texture.h (nearest/bilinear, repeat/clamp)
    |  lighting: src/lighting/lighting.h (ambient + directional, optional specular)
    v
depth buffer          src/core/framebuffer.h
    v
framebuffer           src/core/framebuffer.h  (RGBA8 + float depth)
    v
display output        src/platform/ppm_writer.cpp, src/debug/overlay.cpp
```

Everything above runs on the CPU. There is no graphics API, no CUDA, no
vendor SDK and no GPU dependency anywhere in the renderer.

## Module map

| Path | Contents |
| --- | --- |
| `src/core` | framebuffer/color/depth, vertex format, render config, thread pool |
| `src/math` | Vec2/3/4, Mat4 + transforms, edge function and barycentric coordinates |
| `src/memory` | `Buffer<T>` (typed storage used by meshes) |
| `src/pipeline` | viewport, clip vertices, vertex processor, clipper, triangle setup |
| `src/rasterizer` | tiled rasterizer (binning + per-tile fragment loops) |
| `src/shader` | vertex/fragment shader interfaces, the `Varying` payload, the `ShaderUniforms` block and the built-in shaders |
| `src/texture` | `Texture` + sampler (nearest/bilinear, repeat/clamp), procedural constructors, PPM image I/O |
| `src/lighting` | light and material types, the ambient + directional shading function |
| `src/scene` | mesh, procedural shapes, transform helper |
| `src/camera` | perspective camera (view / projection / view-projection) |
| `src/gpu` | NOVA GPU API: commands, command processor, device |
| `src/platform` | PPM output, console helpers |
| `src/debug` | stats, console report, framebuffer overlay, 3x5 font |
| `tests` | unit and integration tests (`nova_tests`) |
| `examples` | triangle, rotating-cube, shader, texture and lighting demos |
| `benchmarks` | triangle-count scaling and thread scaling benchmark |

## Conventions (fixed and tested)

* Row-major `Mat4`, column-vector maths `M * v` (`src/math/mat4.h`).
* Right-handed world space; the camera looks down `-Z`.
* Projection produces OpenGL-style NDC: `x, y, z` in `[-1, 1]`, visible when `w > 0`.
* Screen space has `+x` right and `+y` **down**, so a counter-clockwise (in NDC)
  front face becomes clockwise on screen. `RenderConfig::frontFaceCCW` selects
  which winding counts as front-facing.
* Window depth is in `[0, 1]`: `0` is the near plane, `1` the far plane, and the
  depth test is "less passes".
* Depth is interpolated linearly in screen space. This is exact rather than an
  approximation: window depth is affine in `1/w`, and `1/w` is linear in screen
  space.
* Every `shader::Varying` field is interpolated **perspective-correctly**:
  `attribute / w` is interpolated linearly and divided by the interpolated `1/w`.
  Adding a field to `Varying` makes it interpolate automatically.
* Clip space, viewport and depth conventions are independent of the shader
  system: a vertex shader only has to write a homogeneous clip position with
  `w > 0` and the varyings it wants downstream.
* Texture space: `uv` is in `[0, 1]` across the image with texel centres at
  `(i + 0.5) / size`; `SampleMode::Nearest`/`Bilinear` and `WrapMode::Repeat`/
  `Clamp` are the sampler state (`src/texture`). Sampling happens in the fragment
  stage, never in the rasterizer.
* Lighting: world-space normals and positions, `light.direction` is the direction
  the light travels (`-direction` points at the light), ambient + diffuse +
  optional Blinn-Phong specular, and shading is unclamped until the 8-bit
  framebuffer write (`src/lighting`).

## Rasterization model

1. `Rasterizer::submit()` bins every triangle into the tiles its bounding box
   covers. Binning happens once per triangle, on the command-processor thread.
2. `Rasterizer::flush()` rasterizes one tile per parallel job from a single
   atomic work counter, so all cores stay busy on tiny and huge triangles alike.
3. Inside a tile, each triangle gets an integer pixel range, edge weights at the
   first pixel centre, and incremental stepping along rows. Edge weights are
   pre-divided by the triangle area, so they *are* the barycentric coordinates.
4. The top-left fill rule (`accept w == 0 only on top/left edges`) guarantees each
   pixel is covered exactly once by adjacent triangles.
5. Depth test, then the fragment shader (with the draw's uniform block), then
   color + depth write. Edge weights are advanced *after* the pixel has been
   shaded, so the depth test and every interpolated value belong to the pixel
   being written.

Fragment order inside a triangle is deterministic (row-major), and the output is
bit-identical between one thread and many - see the
`Rasterizer.ThreadedAndSingleThreadedOutputsMatch` test.

## Frame timing

The device times each stage with `std::chrono::steady_clock`:

* `vertexMs` - per-vertex shader execution for every submitted draw call.
* `clipMs` - *only* the triangles that actually cross a clip plane (fully visible
  geometry takes a fast path that never touches the clipper, so this is normally
  0.0 ms and costs no timestamps in the hot loop).
* `setupMs` - clip calls + triangle setup + tile binning.
* `rasterMs` - tile rasterization + fragment shading.
* `frameMs` - `beginFrame` .. `endFrame`, i.e. clearing + everything above.

## Extension points for later phases

| Phase | Where it plugs in |
| --- | --- |
| 2 shader system | **done** - `shader::VertexShader` / `shader::FragmentShader`, `Varying`, `ShaderUniforms`; see `docs/shaders.md` |
| 3 textures | **done** - `src/texture` storage, nearest/bilinear sampling, wrap modes, PPM loading; see `docs/textures.md`. Mipmaps remain |
| 4 lighting | **done** - `src/lighting` ambient + directional diffuse + Blinn-Phong specular; see `docs/lighting.md`. Light arrays, point/spot lights and shadows remain |
| 5 performance | frustum culling in the command processor, packed bin layout in the rasterizer, SIMD in the fragment loop, texture cache-friendly sampling - profile first (the specular cutoff in `lighting::shade` is the first measured win) |
| 6 debugging | `debug::Overlay` + `debug::printStats` (the counters already drive them) |
| 7 structure | this document plus `README.md` |
