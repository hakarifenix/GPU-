# Performance (Phase 5)

Phase 5 was done in the order the plan asked for: **profile first, then optimise,
then re-measure.** Nothing here is an optimisation because it "looks fast" -
every claim below names the tool that produced it and says plainly when a change
could *not* be measured on this machine.

## Tools

| Command | What it answers |
| --- | --- |
| `nova_benchmark --profile` | where a frame goes (per-stage ms and share) |
| `nova_benchmark --compare` | each optimisation on/off on the same scene |
| `nova_benchmark --culling-scene` | the frustum-culling question, where there is something to cull |
| `nova_benchmark_vertex` | the vertex stage on its own, scalar vs SIMD |

Two rules the tools follow, because this class of machine is noisy:

* **Best of several repeats, not the last run.** Noise only ever adds time, so
  the fastest run is the one closest to the machine's real capability.
* **Interleave when comparing.** `--compare` cycles through all variants in each
  round and keeps the best per variant, so every variant sees the same stretch of
  machine time. Measuring them one after another biased whichever ran last - the
  first version of `--compare` reported a 30% win that disappeared when the order
  changed. That bug is fixed; the rounds are reported in the output.

## Where the frame goes

`nova_benchmark --profile`, 50,000 triangles, 640x480, 32 px tiles (one run;
the shares move by a point or two between runs, and the frame column swings with
VM load - 25.1 ms in a clean-build rerun):

| stage | ms/frame | share |
| --- | --- | --- |
| cull | 0.000 | 0.0% |
| vertex | 0.501 | 1.6% |
| clip+setup+bin | 7.110 | 22.8% |
| raster+frag | 23.188 | 74.4% |
| clear+overhead | 0.352 | 1.1% |
| **frame** | **31.152** | 100% |

The lesson Phase 5 took from this: **74% is the rasterizer**, and this is the
part no flag can switch off. Optimisations that touch 2% of the frame cannot
move it, so they have to be judged on their own stage - which is what
`nova_benchmark_vertex` exists for.

## Frustum culling (measured)

Before a draw is processed, its world bounds are tested against the six frustum
planes; a rejected object costs no vertex transforms, no clipping and no binning.

* `math/bounds.h` - `Aabb` (expand/transform/inside tests) and `BoundingSphere`.
* `pipeline/frustum.h` - Gribb-Hartmann plane extraction from the view-projection
  matrix, conservative box test (positive inside, never rejects visible work).
* `scene::Mesh::bounds()` - object-space bounds computed once and cached;
  geometry is uploaded once and queried every frame.
* `CommandProcessor::executeDraw` runs the test per draw when
  `RenderConfig::frustumCulling` is on (default).
* `stats.timings.cullMs` + `objectsSubmitted/objectsCulled` +
  `trianglesFrustumCulled` measure it.

`nova_benchmark --culling-scene`: 576 objects / 6,912 triangles spread over 220
units, so most of them are outside the view.

| culling | frame ms | FPS | cull ms | vertex | setup | objects culled |
| --- | --- | --- | --- | --- | --- | --- |
| off | 1.00 | 998.6 | 0.000 | 0.085 | 0.381 | 0 |
| on | 0.57 | 1,758.2 | 0.041 | 0.003 | 0.014 | 560 of 576 |

**1.76-1.77x frame time, 97.2% of objects culled** (96 triangles still visible)
for about 0.04 ms of testing, reproduced in three separate runs (1.76x, 1.76x,
1.77x).

The lattice scene in `--compare` shows *no* gain from culling, and that is the
correct answer: every object there fills the view, so there is nothing to reject.
A culling test that always rejects something would be the broken one.

## Vertex stage (measured)

Two changes, and they can be separated because they have separate tools:

1. **Batching** (`VertexShader::shadeBatch`, `ClipVertex` deriving from
   `VertexOutput`): one call per draw instead of one `shade()` call plus two
   struct copies per vertex, and the draw's `viewProjection * model` product is
   computed once per stream instead of once per vertex.
2. **4-wide SSE2 arithmetic** (`math/simd.h`, `TransformVertexShader` overrides
   `shadeBatch`), with a scalar batch that is the same computation minus the
   SIMD - so `uniforms.simd` compares arithmetic width only.

`nova_benchmark_vertex` (200,000 vertices, 60 iterations):

| variant | best ns/vertex | median ns/vertex |
| --- | --- | --- |
| scalar | 6.36 | 6.49 |
| simd | 4.36 | 4.64 |

**1.46-1.49x on the isolated vertex stage** - repeated runs gave 6.31-6.36
ns/vertex scalar against 4.24-4.36 ns/vertex SIMD, so the direction and the
size of the effect survive the noise.

In a whole frame (`--profile` above) the stage is now **0.50 ms / 1.6%** of the
frame. Against the pre-Phase-5 renderer, whose vertex column measured
**2.17 ms best / 2.40 ms median** in five interleaved rounds of the same scene,
that is the batching win on top of the SIMD - about 4x for the stage, and
nothing measurable end to end, because the stage is 2% of the frame to begin
with.

`tests/test_shaders.cpp: ScalarBatchIsBitwisePerVertexShadeAndSimdBatchesAgree`
pins the contract: the scalar batch is *bitwise* what `shade()` returns (it is
the fallback on non-SSE2 hosts) and the SIMD batch agrees to rounding.

## Not measurable on this machine

Reported as not measurable rather than as a win:

* **Flat tile bins** (`binOffsets_`/`binIndices_` counting sort instead of one
  `std::vector` per tile). Structurally cheaper - no per-tile allocation, linear
  iteration, arrays reused across frames - but binning is only a slice of the
  22.8% setup stage, which measured 4.8-7.7 ms on the old renderer and 5.8-8.2 ms
  on the new one over five interleaved rounds. The distributions overlap
  completely: **no measurable difference.**
* **End-to-end frame time at 50,000 triangles:** 28.3-33.9 ms (pre-Phase-5) vs
  29.8-32.3 ms (now), best of five interleaved rounds each. Overlapping: the
  raster stage dominates and was not changed.
* **Thread scaling.** This VM reports 2 CPUs but gives no parallel speedup even
  for a plain CPU-bound loop, so the 1/2/4/8-thread table stays flat. The
  threaded path is verified for *correctness* instead
  (`Rasterizer.ThreadedAndSingleThreadedOutputsMatch`: pixel-identical output).

## What would pay next

Profile first, again: the 74% says the next real win is in the fragment loop
(overdraw, early-Z, a tighter inner loop) rather than in any stage below 5%.
