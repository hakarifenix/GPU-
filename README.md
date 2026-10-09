# NOVA GPU — CPU SOFTWARE RENDERER

A real, from-scratch software GPU: a CPU-based graphics processor that renders
3D geometry with its own command processor, geometry stages, tiled rasterizer,
depth buffer and framebuffer.

**No CUDA. No NVIDIA/AMD GPU. No external graphics API in the core renderer.**
Every pixel is produced by `nova::rasterizer::Rasterizer` running on the CPU.
This does not create physical GPU hardware - it is a CPU implementation of a
simplified GPU architecture.

* Language: C++17
* Build system: CMake (3.16+)
* Dependencies: none (standard library + threads only)
* Target: Intel Core i5-6300U class machines, 8 GB RAM, Windows (also builds on Linux/macOS)
* Status: **Phases 1-4 complete and tested** (core MVP, shader system, textures, lighting)

```
application -> NOVA GPU API -> command processor -> vertex processing ->
clipping -> triangle setup -> software rasterizer -> fragment processing ->
depth buffer -> framebuffer -> display output (PPM / debug overlay)
```

## Phase 1 status

| Requirement | State |
| --- | --- |
| Framebuffer, color buffer, depth buffer | done (`src/core/framebuffer.*`) |
| Vertex structure, index buffer | done (`src/core/vertex.h`, `src/scene/mesh.h`) |
| Triangle rasterization | done (tiled, `src/rasterizer/rasterizer.cpp`) |
| Barycentric coordinates | done (`src/math/math_utils.h`, edge equations in triangle setup) |
| Depth testing | done (less-passes, `[0,1]` window depth) |
| Back-face culling | done (winding based, configurable) |
| Basic clipping | done (near plane `z + w >= 0` and `w >= 0`, pre-divide, Sutherland-Hodgman) |
| Perspective projection | done (`math::perspective`, OpenGL-style NDC) |
| Camera | done (`src/camera/camera.*`, view/projection/view-projection) |
| Transform matrices | done (`src/math/mat4.h`, `src/scene/transform.h`) |
| Multithreaded tile rendering | implemented (`src/core/thread_pool.h`); see the note on measured scaling below |
| Rotating 3D cube demo | done (`examples/cube_demo.cpp`) |
| Unit tests | 45 tests / 8,450 checks, all passing |

The cube demo renders a genuinely rasterized, depth-tested, perspective-projected
cube. It is not a pre-rendered image and no external renderer is involved.

## Phase 2 status - shader system

| Requirement | State |
| --- | --- |
| `VertexShader` / `FragmentShader` interfaces | done (`src/shader/vertex_shader.h`, `fragment_shader.h`) |
| Vertex transformation in a shader | done (`TransformVertexShader`, the device default) |
| Color interpolation | done (`ColorFragmentShader`, perspective-correct) |
| Texture coordinates in the shader | done (uv varying; `CheckerFragmentShader` proves the seam) |
| Basic lighting through the interface | done (`LambertFragmentShader`: ambient + directional diffuse from the interpolated normal) |
| Small internal representation, no GLSL | done (`shader::Varying` + `shader::ShaderUniforms`, plain structs) |
| Per-draw uniform block | done (`Rasterizer::addUniformBlock`, captured when the command is recorded) |
| Shader unit tests | done (`tests/test_shaders.cpp`, 9 tests) |
| Shader demo | done (`examples/shader_demo.cpp`, 4 images from one scene) |

See `docs/shaders.md` for the interface, the interpolation guarantees and how to
write an application shader.

Note that Phase 2 deliberately stopped at the seam: `CheckerFragmentShader` is a
procedural stand-in for real texture sampling, and `LambertFragmentShader` was the
minimal light model that proved the interface can do lighting. Both are now
backed by real modules - see Phases 3 and 4 below.

## Phase 3 status - textures

| Requirement | State |
| --- | --- |
| Texture storage + sampling | done (`src/texture/texture.h`) |
| Nearest filtering | done (`SampleMode::Nearest`) |
| Bilinear filtering | done (`SampleMode::Bilinear`, 4 taps at texel centres) |
| UV interpolation | already perspective-correct in `shader::Varying::uv`; exercised end to end |
| Texture loading | done (dependency-free **P6/P3 PPM** reader, `src/texture/image_io.*`) |
| Sampling in the fragment stage | done (`TexturedFragmentShader`, `PhongFragmentShader::setAlbedoTexture`) |
| Wrap modes | done (`Repeat` tiling, `Clamp` edge stretch) |
| Texture tests | done (`tests/test_texture.cpp`, 10 tests incl. a PPM round trip) |
| Mipmaps | **not yet** - explicitly "later" in the phase plan |

## Phase 4 status - lighting

| Requirement | State |
| --- | --- |
| Directional light | done (`lighting::DirectionalLight`) |
| Ambient light | done (`lighting::AmbientLight`) |
| Diffuse lighting | done (`lighting::shade`, cosine falloff on the interpolated normal) |
| Specular lighting | done (`Material::specular`/`shininess`, Blinn-Phong; set `specular = 0` for Lambert) |
| CPU-friendly | done (two dots + multiplies for Lambert; `pow()` guarded, see `docs/lighting.md`) |
| Lighting shaders | `LambertFragmentShader`, `PhongFragmentShader` (materials + texture albedo) |
| Lighting tests | done (`tests/test_lighting.cpp`, 9 tests) |
| Sphere mesh for smooth shading | done (`scene::makeSphere`) |

## Build

Requirements: CMake 3.16+ and a C++17 compiler. No packages to download.

### Windows — Visual Studio (MSVC)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Executables are written to `build\bin\` (both Debug and Release use that folder,
so a later build overwrites the earlier one).

### Windows — MinGW-w64 / MSYS2

```bat
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Useful options: `-DNOVA_BUILD_TESTS=OFF`, `-DNOVA_BUILD_EXAMPLES=OFF`,
`-DNOVA_BUILD_BENCHMARKS=OFF`, `-DNOVA_ENABLE_NATIVE=ON` (optimize for the host
CPU; recommended when benchmarking), `-DNOVA_FAST_MATH=ON` (off by default).

## Run

```bash
# step 1: one CPU-rasterized colored triangle (Gouraud interpolation)
./build/bin/nova_triangle_demo --out triangle.ppm

# step 2/3: rotating, depth-tested, perspective cube
./build/bin/nova_cube_demo --out cube.ppm
./build/bin/nova_cube_demo --frames 120 --animate --out cube.ppm   # cube_0000.ppm ...
./build/bin/nova_cube_demo --flythrough --frames 13 --animate --out fly.ppm  # near-plane clipping
./build/bin/nova_cube_demo --debug --frames 30                    # per-frame console stats

# phase 2: the same scene through four different shader pairs
./build/bin/nova_shader_demo --width 640 --height 480
#   -> shader_color.ppm shader_lambert.ppm shader_checker.ppm shader_wave.ppm

# phase 3: nearest vs bilinear, uv grid, and a textured+lit surface
./build/bin/nova_texture_demo --width 640 --height 480
#   -> texture_nearest.ppm texture_bilinear.ppm texture_uvgrid.ppm texture_lit.ppm
./build/bin/nova_texture_demo --texture assets/nova_uv.ppm     # load another PPM

# phase 4: sphere/cube/floor under four lighting setups
./build/bin/nova_lighting_demo --width 640 --height 480
#   -> lighting_lambert.ppm lighting_phong.ppm lighting_backlit.ppm lighting_colored.ppm
```

The texture demo loads `assets/nova_checker.ppm` and `assets/nova_uv.ppm`; if they
are missing it falls back to the same images generated procedurally, so no demo
requires a data file to run.

On Windows the executable is `build\bin\nova_cube_demo.exe` (add `--config Release`
to `cmake --build` only, not to the run command). Use `--help` for all flags.
The debug HUD is drawn into the image; `--no-overlay` turns it off. Convert PPM
to PNG with `python3 tools/ppm_to_png.py cube.ppm cube.png` (see `docs/output.md`).

## Test

```bash
./build/bin/nova_tests                 # or: ctest --test-dir build --output-on-failure
```

78 tests covering vector maths, matrix maths, barycentric coordinates, the
viewport transform, triangle setup (winding, degeneracy, bounding box, weights),
triangle rasterization and the top-left fill rule, depth testing, back-face
culling, single- vs multi-threaded determinism, near-plane clipping (unit and
end-to-end), procedural-geometry winding (cube and sphere), the shader system
(default vs explicit vertex shader, application vertex shaders reaching the
pipeline, interpolated uv/screen position/depth, perspective-correct
interpolation against hand-computed values, Lambert lighting, per-draw uniform
capture), texture storage/filtering/wrapping, PPM loading round-trips and
rejection of malformed files, textured quads rendered end to end, and the
lighting model (ambient/diffuse/specular maths, gradient on a sphere, localised
highlight, and the specular cutoff staying below one 8-bit step).

## Benchmark

```bash
./build/bin/nova_benchmark                     # scenes of ~100 / 1,000 / 10,000 / 50,000 triangles
./build/bin/nova_benchmark --threads 1         # force single-threaded rasterization
./build/bin/nova_benchmark --tile 16           # different tile size
./build/bin/nova_benchmark --no-thread-sweep   # skip the 1/2/4/8-thread comparison
```

Each scene is the same lattice of cubes scaled to fill the same part of the view,
so the scenes differ in triangle count and overdraw, not in screen coverage.
Scene construction is excluded from the timings; five warm-up frames are
discarded. Reported memory is the geometry plus the framebuffer and the
rasterizer working set.

> **Measured scaling caveat:** tile rasterization is parallelized, but a speedup
> only shows up on a machine with more than one usable CPU core. The verification
> VM reports 2 CPUs yet gives no parallel speedup even for a plain CPU-bound
> floating-point loop, so the thread-scaling table there is flat. On a real
> multi-core machine expect roughly `1/(cores)` raster time.

## Measured results (Phase 1 verification run)

Environment: Linux x86-64 container on a 2-vCPU cloud VM (no GPU, no SIMD
flags beyond the default `-O2`), 640x480 framebuffer, 32x32 tiles. Three runs of
`nova_benchmark --no-thread-sweep`; the VM is noisy, so the full spread is shown.

| triangles | frame time (ms) | FPS |
| --- | --- | --- |
| 100 | 0.21 / 0.22 / 0.37 | 2,682 - 4,697 |
| 1,000 | 0.90 / 0.93 / 1.30 | 768 - 1,112 |
| 10,000 | 7.00 / 8.61 / 9.07 | 110 - 143 |
| 50,000 | 32.04 / 35.01 / 35.11 | 28.5 - 31.2 |

For the 50,000-triangle scene the per-stage split from a single run was
`vertex 0.79 ms`, `clip+setup 7.17 ms`, `raster+frag` the remainder, with
1,032,621 fragments tested and 474,722 shaded per frame. Memory: 3.6 MiB of
geometry plus ~11.7 MiB of framebuffer + rasterizer working set.

The 640x480 cube demo (12 triangles, 53k shaded fragments) runs at roughly
1.1 ms/frame here; the single-triangle demo at ~0.9 ms/frame, dominated by
clearing 614 KiB of color and 1.2 MiB of depth.

Single-thread vs 2-thread rasterization on this VM: 31.6 ms vs 37.5 ms for the
50,000-triangle scene - **no speedup**, because this VM gives no parallel
speedup even for a plain CPU-bound loop (measured separately). The multithreaded
path is verified for correctness instead: `Rasterizer.ThreadedAndSingleThreadedOutputsMatch`
asserts that a 4-thread render is pixel-identical to a single-threaded render and
shades exactly the same number of fragments.

## Fixed during Phase 2 verification

Two real bugs surfaced while validating the shader system and are fixed in this
tree:

* **Rasterizer interpolation was one pixel off.** The per-pixel edge weights were
  advanced before the fragment was shaded, so the depth test and every
  interpolated attribute used the value of the next pixel to the right. Coverage
  was unaffected (the coverage test ran before the step), which is why the Phase 1
  tests passed. Weights are now stepped at the end of the loop;
  `Shaders.PerspectiveCorrectUvInterpolation` pins the interpolated value at a
  known pixel.
* **Four of the cube's six faces were wound backwards** in
  `scene::makeCube` (±X and ±Y), so back-face culling removed them whenever they
  faced the camera - visible as a hole in the cube when viewed from above. The
  face tables now match their normals, and `Scene.CubeFaceWindingMatchesNormals`
  fails if that ever regresses.

## Measured results (Phase 3/4 verification run)

Same noisy 2-vCPU cloud VM, Linux x86-64, `-O2`, 32x32 tiles.

| demo (640x480 unless noted) | frame time | fragments shaded |
| --- | --- | --- |
| `nova_texture_demo` nearest, 3x3 checker | ~9 ms (480x360) | 124,848 |
| `nova_texture_demo` bilinear, same scene | ~11 ms (480x360) | 124,848 |
| `nova_texture_demo` textured + lit | ~6 ms (480x360) | 100,176 |
| `nova_lighting_demo` lambert | ~5 ms (480x360) | 124,059 |
| `nova_lighting_demo` phong | ~5.7 ms (480x360) | 124,059 |

Bilinear costs about 20% more than nearest on this scene (four texel reads instead
of one) and both are dominated by the fragment loop, not by the fetch.

The one measured optimisation of these phases is the specular cutoff in
`lighting::shade`: evaluating `pow()` for every lit fragment made the Phong sphere
**43 ms/frame** against 4 ms for the same scene with `specular = 0`. Skipping the
term where it cannot reach one 8-bit step (`dot(N,H) < 0.5` with
`shininess >= 8`) brought it to **5.7 ms/frame** with no visible change, and the
bound is asserted by `Lighting.SpecularCutoffStaysBelowOneLsb`.

## Project layout

See `docs/architecture.md` for the module map, the math conventions, the tiling
model and where later phases plug in. `docs/shaders.md` covers the shader
interface, `docs/textures.md` sampling and image loading, `docs/lighting.md` the
lighting model, and `docs/output.md` PPM output and the debug overlay.

## Roadmap

* **Phase 1 — core MVP: done.** Framebuffer/color/depth buffers, vertex+index
  buffers, triangle rasterization, barycentric coordinates, depth testing,
  back-face culling, clipping, perspective projection, camera, transform matrices,
  multithreaded tile rendering, rotating-cube demo, unit tests.
* **Phase 2 — shader system: done.** Vertex/fragment shader interfaces, the
  `Varying` inter-stage payload, per-draw uniforms, built-in transform / color /
  Lambert / checker shaders, shader demo and tests (`docs/shaders.md`).
* **Phase 3 — textures: done.** Storage, nearest + bilinear filtering, repeat/clamp
  wrap modes, dependency-free PPM (P6/P3) loading, textured shaders, assets, demos
  and tests (`docs/textures.md`). Mipmaps remain for a later pass.
* **Phase 4 — lighting: done.** Ambient + directional diffuse with an optional
  Blinn-Phong highlight, a material/light module, lit + textured-lit shaders, a
  sphere mesh, demos and tests (`docs/lighting.md`).
* **Phase 5 — performance**: profiling first, then frustum culling, batching,
  visibility checks, cache-friendly layouts, SIMD where it measurably pays.
* **Phase 6 — debugging**: FPS, frame time, triangle counts, threads, resolution,
  renderer name (already implemented for Phase 1).
* **Phase 7 — project structure**: modular layout and tests (already in place).
