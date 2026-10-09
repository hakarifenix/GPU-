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

## Debug overlay

`--no-overlay` disables it; otherwise the HUD is drawn into the framebuffer by
`nova::debug::Overlay` (plain framebuffer writes, no API involvement):

* renderer name (`NOVA GPU - CPU SOFTWARE RENDERER`)
* FPS and frame time
* triangles submitted / rasterized / culled
* fragments tested / shaded
* thread count and framebuffer resolution

The overlay uses a generated 3x5 bitmap font (`src/debug/font3x5.h`, regenerate
with `python3 tools/gen_font3x5.py`). Non-ASCII characters are folded to `-` in
the bitmap HUD; the console report prints the real UTF-8 renderer name.

## Console report

Every example and the benchmark print the same numbers as text, which is the
form to use for automated comparisons (`colorFromVec4`-free, no image parsing):

```
NOVA GPU — CPU SOFTWARE RENDERER
  frame       :     1.09 ms   (  918.7 FPS)
  vertex      :    0.005 ms
  clip+setup  :    0.023 ms   (clipped 0 tris)
  raster+frag :    1.083 ms
  triangles   : 12 submitted, 6 rasterized, 6 culled
  fragments   : 53,712 tested, 53,531 shaded
  threads     : 2   tiles: 20x15 @ 32 px
  memory      : framebuffer+working set 2.35 MiB, geometry 0.00 MiB
```
