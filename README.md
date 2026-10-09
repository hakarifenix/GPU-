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
* Status: **Phases 1-6 complete and tested** (core MVP, shader system, textures,
  lighting, performance, debugging)

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

## Phase 5 status - performance

| Requirement | State |
| --- | --- |
| Profile before optimising | done (`nova_benchmark --profile`, `--compare`, `--culling-scene`) |
| Frustum culling | done (`src/pipeline/frustum.h`, per-draw AABB test in the command processor) |
| Batching | done (one `shadeBatch()` per draw, matrices multiplied once, no per-vertex copy) |
| Visibility checks | done (cached object-space bounds on `scene::Mesh`, `Aabb`/`BoundingSphere`) |
| Cache-friendly layouts | done (flat CSR tile bins instead of a vector per tile) |
| SIMD where it measurably pays | done (`src/math/simd.h`, 4-wide SSE2 vertex transform, scalar fallback) |
| Performance tests | done (`tests/test_frustum.cpp`, batch equivalence in `tests/test_shaders.cpp`) |

Measured, with the numbers and the method: `docs/performance.md`. Two changes
are measurable on this machine - frustum culling is **1.76x** on a scene with
something to reject, and SIMD is **1.46x** on the isolated vertex stage. The
tile-bin layout and the end-to-end frame time are **not measurable here**, and
the document says so rather than claiming them.

## Phase 6 status - debugging

| Requirement | State |
| --- | --- |
| FPS, frame time (with window min/max) | done |
| Triangle counts: submitted / visible / culled | done |
| Thread count and resolution | done |
| Renderer name | done since Phase 1 |
| Object culling, fragment counts | done |
| Shader names + memory (`--verbose-hud`) | done (`SHADER T + P`) |
| Frame-time graph (`--verbose-hud`) | done (rolling 120-frame window, average marked) |
| Console report with the same numbers | done (`debug::printStats`) |
| Debug tests | done (`tests/test_debug.cpp`, 7 tests) |

The HUD is drawn by the renderer itself into the framebuffer, which is the point
of a software GPU: `Overlay::hudWidth()`/`hudHeight()` let an application lay out
around it. Details in `docs/output.md`.

## Phase 7 status - project structure

| Requirement | State |
| --- | --- |
| Modular layout, one directory per stage | done (`src/` - 13 modules, see the table below) |
| Renderer isolated from demos/tests/benchmarks | done (`nova_core` static library, every consumer links it) |
| Single test runner wired into ctest | done (`nova_tests`, `NOVA_BUILD_TESTS`) |
| Consistent output location for every executable | done (`build/bin/`) |
| Build documentation | done (`docs/build.md`) |
| Installable, distributable artifact | done (`cmake --install`, CPack archive, `tools/package.sh`) |

## Download

Three ways to get it:

**1. Source, no build tools needed for reading** - GitHub's archive of the
branch:

```
https://github.com/hakarifenix/GPU-/archive/refs/heads/main.zip
```

**2. Clone it** (to build and run):

```bash
git clone https://github.com/hakarifenix/GPU-.git
cd GPU-
```

**3. A ready-to-run archive** (binaries + docs + demo textures) - one command
builds it, tests it and packs it:

```bash
tools/package.sh
# -> build-release/nova-gpu-0.1.0-<os>-<arch>.tar.gz  (+ .sha256)
```

The archive is self-contained: extract it anywhere, then
`./bin/nova_tests` and `./bin/nova_cube_demo` run from the extracted tree
without installing anything (`docs/build.md` has the Windows commands).

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

# phase 6: the extended debug HUD (shaders, memory, frame-time graph)
./build/bin/nova_cube_demo --verbose-hud --out cube.ppm
./build/bin/nova_lighting_demo --verbose-hud
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

93 tests / 12,019 checks covering vector maths, matrix maths, barycentric coordinates, the
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

Phase 5/6 adds: AABB expansion and transformed bounds, frustum plane extraction
against hand-computed inside/outside points, an off-screen object being culled,
culling never changing a rendered image, batch equivalence (the scalar batch is
bitwise `shade()`, the SIMD batch agrees to rounding), the HUD footprint leaving
the rest of the frame untouched, lowercase HUD text, the frame-time window,
shader-name and memory statistics, and the frame-time graph staying inside its
rectangle.

## Benchmark

```bash
./build/bin/nova_benchmark                     # scenes of ~100 / 1,000 / 10,000 / 50,000 triangles
./build/bin/nova_benchmark --threads 1         # force single-threaded rasterization
./build/bin/nova_benchmark --tile 16           # different tile size
./build/bin/nova_benchmark --no-thread-sweep   # skip the 1/2/4/8-thread comparison
./build/bin/nova_benchmark --profile           # per-stage breakdown of a frame
./build/bin/nova_benchmark --compare           # frustum culling / SIMD on vs off (interleaved)
./build/bin/nova_benchmark --culling-scene     # 576 objects spread wide: the culling question
./build/bin/nova_benchmark_vertex              # the vertex stage alone, scalar vs SIMD
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

## Measured results (Phase 5/6 verification run)

Same noisy 2-vCPU cloud VM, Linux x86-64, `-O3` (Release), 640x480, 32 px tiles.
Method and per-run detail: `docs/performance.md`.

**Where the frame goes** (`--profile`, 50,000 triangles): raster+frag 74.4%,
clip+setup+bin 22.8%, vertex 1.6%, clear+overhead 1.1%, cull 0.0%.

**Frustum culling** (`--culling-scene`, 576 objects / 6,912 triangles over 220
units), reproduced in two separate runs:

| culling | frame ms | FPS | cull ms | vertex | setup | objects culled |
| --- | --- | --- | --- | --- | --- | --- |
| off | 1.00 | 998.6 | 0.000 | 0.085 | 0.381 | 0 |
| on | 0.57 | 1,758.2 | 0.041 | 0.003 | 0.014 | 560 / 576 |

**1.76-1.77x frame time for 97.2% of objects culled** (96 triangles still
visible) for 0.04 ms of testing, across three separate runs.

**Vertex stage** (`nova_benchmark_vertex`, 200,000 vertices, 60 iterations):
scalar 6.36 / 6.49 ns per vertex (best/median), SIMD 4.36 / 4.64 - **1.46x**
(1.46-1.49x over repeated runs). In a whole frame the stage is 0.50 ms / 1.6% of the frame,
against 2.17 ms best / 2.40 ms median for the pre-Phase-5 renderer measured in
five interleaved rounds of the same scene.

**Not measurable here** (stated as such in `docs/performance.md`, not claimed as
wins): the flat tile-bin layout, and the end-to-end frame time at 50,000
triangles - 28.3-33.9 ms before vs 29.8-32.3 ms after, best of five interleaved
rounds, overlapping distributions. The raster stage is 74% of the frame and was
not changed.

Thread scaling remains flat on this VM (no parallel speedup even for a plain
CPU-bound loop), so the threaded path is verified for correctness:
single-threaded output is pixel-identical to multi-threaded.

## Project layout

See `docs/architecture.md` for the module map, the math conventions, the tiling
model and where later phases plug in. `docs/shaders.md` covers the shader
interface, `docs/textures.md` sampling and image loading, `docs/lighting.md` the
lighting model, `docs/output.md` PPM output and the debug overlay, and
`docs/performance.md` how the renderer is measured and what changed, and
`docs/build.md` how to build, run and package it.

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
* **Phase 5 — performance: done.** Profiling first, then frustum culling,
  visibility/bounds checks, batching, flat tile bins and the 4-wide SIMD vertex
  path, with every number measured (or reported as not measurable) in
  `docs/performance.md`.
* **Phase 6 — debugging: done.** Extended HUD (frame-time window, triangle and
  object culling counts, shader names, memory, frame-time graph), `--verbose-hud`
  on every demo, and the matching console report (`docs/output.md`).
* **Phase 7 — project structure: done.** 13-module `src/` layout behind the
  `nova_core` library, one test runner wired into ctest, `docs/build.md`, and a
  distributable archive: `tools/package.sh` (build -> test -> CPack) produces a
  self-contained `nova-gpu-<version>-<os>-<arch>.zip`/`.tar.gz` with a SHA-256
  checksum - see **Download** above.
