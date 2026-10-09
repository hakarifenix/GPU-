# Display output

NOVA GPU writes the framebuffer to a **binary PPM (P6)** file. PPM is used
because it needs no third-party encoder, is lossless, and can be produced by a
few lines of code on any platform. A windowed presenter is a platform concern and
is deliberately not part of the renderer.

```bash
nova_cube_demo --out cube.ppm        # single frame
nova_cube_demo --frames 120 --animate --out cube.ppm   # cube_0000.ppm, ...
```

## Viewing the images

* **Windows**: IrfanView, GIMP, XnView or any image viewer that understands PPM.
* **Any platform with Python 3**: `tools/ppm_to_png.py` converts to PNG using
  only the standard library:

  ```bash
  python3 tools/ppm_to_png.py cube.ppm cube.png
  ```

## Debug overlay (Phase 6)

`--no-overlay` disables it; otherwise the HUD is drawn into the framebuffer by
`nova::debug::Overlay` (plain framebuffer writes, no API involvement). Every
demo accepts `--verbose-hud` for the extended readout:

**Compact HUD** (default)

* renderer name (`NOVA GPU - CPU SOFTWARE RENDERER`)
* FPS, frame time, and the min/max of the recent frame-time window
* triangles submitted / visible / culled (back-face and frustum together)
* frustum-culled triangles, and objects culled / submitted
* fragments tested / shaded
* thread count and framebuffer resolution

**Verbose HUD** (`--verbose-hud`) adds

* the shader pair the last frame actually used
  (`SHADER TRANSFORMVERTEXSHADER + PHONGFRAGMENTSHADER`) - the fastest answer
  to "why is it lit / why is it textured?"
* framebuffer vs. working-set memory (`MEM FB 2.34 MIB WORK 2.35 MIB`)
* a frame-time graph: one bar per recent frame scaled to the window maximum,
  with the window average marked

`Overlay::hudWidth()` / `hudHeight(verbose)` report the panel footprint, so an
application can lay out around it instead of hard-coding the metrics (the tests
use it to assert the HUD touches nothing outside its own rectangle).

The overlay uses a generated 3x5 bitmap font (`src/debug/font3x5.h`, regenerate
with `python3 tools/gen_font3x5.py`). The font is uppercase-only, so lowercase
input is folded to its uppercase glyph - a camel-case shader name reads as
`TRANSFORMVERTEXSHADER` rather than three stray capitals. Non-ASCII characters
are folded to `-`; the console report prints the real UTF-8 renderer name.

## Console report

Every example and the benchmark print the same numbers as text, which is the
form to use for automated comparisons (`colorFromVec4`-free, no image parsing):

```
NOVA GPU — CPU SOFTWARE RENDERER
  frame       :     1.59 ms   ( 629.1 FPS)
  cull        :    0.000 ms   (0 objects culled)
  vertex      :    0.014 ms
  clip+setup  :    0.046 ms   (clipped 0 tris)
  raster+frag :    1.486 ms
  triangles   : 12 submitted, 6 visible, 6 culled (frustum 0)
  objects     : 1 submitted, 0 culled
  fragments   : 47,140 tested, 47,140 shaded
  threads     : 2   tiles: 20x15 @ 32 px (186 tile-triangle pairs)
  shaders     : TransformVertexShader + ColorFragmentShader
  memory      : framebuffer+working set 2.35 MiB, geometry 0.00 MiB
```

The culling and object lines, the visible/frustum split and the shader names
are Phase 6 additions; per-frame table output (`--debug` in the cube demo)
adds a `visible` column for the same reason.
