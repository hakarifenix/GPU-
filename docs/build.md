# Building, running and packaging

NOVA GPU has no dependencies to download: a C++17 compiler and CMake 3.16+ are
the whole requirement list. Everything below runs from the repository root.

## Quick start (Linux / macOS)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bin/nova_tests          # 93 tests / 12,019 checks
./build/bin/nova_cube_demo --out cube.ppm
```

## Windows — Visual Studio (MSVC)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\bin\nova_tests.exe
build\bin\nova_cube_demo.exe --out cube.ppm
```

## Windows — MinGW-w64 / MSYS2

```bat
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Executables always land in `build/bin/` (all configurations use that folder).

## Options

| CMake option | Default | Effect |
| --- | --- | --- |
| `NOVA_BUILD_TESTS` | ON | build `nova_tests` and register it with ctest |
| `NOVA_BUILD_EXAMPLES` | ON | build the five demos |
| `NOVA_BUILD_BENCHMARKS` | ON | build `nova_benchmark` and `nova_benchmark_vertex` |
| `NOVA_ENABLE_NATIVE` | OFF | `-march=native` / `/arch:AVX2` — recommended when benchmarking |
| `NOVA_FAST_MATH` | OFF | fast-math flags; off by default because they can break the NaN tests |

Example: `cmake -S . -B build -DNOVA_BUILD_EXAMPLES=OFF`.

## Layout

```
src/         the renderer, one directory per pipeline stage
  core       framebuffer, color, depth, vertex format, config, thread pool
  math       Vec2/3/4, Mat4, bounds (AABB/sphere), SIMD helpers
  memory     typed Buffer<T>
  pipeline   viewport, vertex processor, clipping, triangle setup, frustum
  rasterizer tiled rasterizer and its flat tile bins
  shader     vertex/fragment shader interfaces, varyings, built-in shaders
  texture    sampler + PPM image I/O
  lighting   lights, materials, shading model
  scene      mesh and procedural shapes
  camera     perspective camera
  gpu        the NOVA GPU API: commands, command processor, device
  platform   PPM output, console helpers
  debug      stats, console report, framebuffer HUD, bitmap font
tests/       unit and integration tests (nova_tests, also run by ctest)
examples/    triangle, cube, shader, texture and lighting demos
benchmarks/  scaling benchmark + profiler and the isolated vertex stage
assets/      checked-in demo textures (PPM)
docs/        this documentation set
tools/       package.sh, ppm_to_png.py, gen_font3x5.py
```

## Packaging (the downloadable archive)

```bash
tools/package.sh
```

That one command does the whole release flow: configure a clean **Release**
build, compile, run `nova_tests` **and** `ctest`, then run CPack. It prints the
archive and its SHA-256 checksum when it finishes.

Or drive CPack yourself:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --build build --target package
```

On Windows (Visual Studio generator):

```bat
cmake --build build --config Release --target package
```

The result is `build/nova-gpu-<version>-<os>-<arch>.zip` (Windows) or
`.tar.gz` (Linux/macOS) next to a `.sha256` file. The archive is self-contained:

```
nova-gpu-0.1.0-Linux-x86_64/
  bin/       all demos, benchmarks and nova_tests
  docs/      the markdown documentation
  assets/    the demo textures
  README.md
```

Extract it anywhere and run it — no installation:

```bash
tar -xzf nova-gpu-0.1.0-Linux-x86_64.tar.gz
cd nova-gpu-0.1.0-Linux-x86_64
./bin/nova_tests                 # works from the extracted tree
./bin/nova_texture_demo          # finds ./assets automatically
```

## Notes and troubleshooting

* **Build in parallel is memory-hungry**: on a small machine use
  `cmake --build build -j2` — a `Killed` (exit 137) build usually means the
  linker ran out of memory, not a code error.
* **Timing**: a Debug build is roughly 10-20x slower; benchmark in Release with
  `NOVA_ENABLE_NATIVE=ON`.
* **Threads**: `nova_benchmark --threads 1` forces single-threaded rasterization
  for comparison. On a machine with one usable core the thread table is flat -
  see `docs/performance.md`.
* **No cmake?** Install CMake 3.16+ (Linux: your package manager, macOS: brew,
  Windows: cmake.org). `tools/package.sh` also falls back to
  `tools/venv-cmake/bin/cmake` if you keep a local install there.
