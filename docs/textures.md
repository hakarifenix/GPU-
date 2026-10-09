# NOVA GPU textures (Phase 3)

Textures are CPU memory plus a sampler state. Sampling happens in the fragment
stage: the rasterizer only interpolates `Varying::uv` (perspective-correctly),
and the shader decides what to fetch. That keeps one texture object usable from
every shader and every filter mode.

```
core::Vertex::uv -> shader::Varying::uv -> (clipping) -> (triangle setup: uv / w)
   -> rasterizer: perspective-correct uv per fragment -> FragmentShader::sample()
```

## Files

| File | Contents |
| --- | --- |
| `src/texture/texture.h` | `Texture`, `Sampler`, `SampleMode`, `WrapMode` |
| `src/texture/texture.cpp` | storage, procedural constructors, nearest + bilinear sampling, wrap modes |
| `src/texture/image_io.h` | `loadPpm`, `decodePpm`, `savePpm`, `encodePpm` |
| `src/texture/image_io.cpp` | dependency-free PPM (P6 binary / P3 ASCII) reader + writer |
| `src/shader/texture_shader.h` | `TexturedFragmentShader` (unlit) |
| `src/shader/phong_shader.h` | textured **and** lit (`setAlbedoTexture`) |

## Sampling

```cpp
texture::Texture checker = texture::Texture::checker(64, 64, 8, core::kWhite, core::kBlack);

texture::Sampler sampler;
sampler.filter = texture::SampleMode::Bilinear;   // or Nearest
sampler.wrap   = texture::WrapMode::Repeat;       // or Clamp

shader::TexturedFragmentShader shader(checker, sampler);
device.draw(mesh, model, camera.viewProjection(), shader);
```

* **Nearest** picks the texel containing the uv coordinate - cheapest, blocky.
* **Bilinear** samples at texel centres (`u * width - 0.5`) and blends the four
  surrounding texels, which is the "smooth but affordable" option: four texel
  reads, three lerps, no dependent chain.
* **Repeat** wraps uv into `[0, 1)`, so `u = 1.25` samples as `0.25` and tiling
  works; **Clamp** pins uv to the edge, stretching the border texel.
* An empty texture samples as opaque magenta, so a missing texture is visible
  instead of silently black.

## Image files

`texture::loadPpm(path)` reads binary **P6** and ASCII **P3** PPM, skipping `#`
comments in the header, scaling `maxval < 255`, and rejecting malformed or
truncated data (an empty texture is returned, never a partial read).
`texture::savePpm` writes P6; both directions are used by the round-trip test
`Texture.PpmRoundTripPreservesPixels`.

The demos ship two small textures in `assets/`:

| File | What it is for |
| --- | --- |
| `assets/nova_checker.ppm` | 64x64 two-tone checker - nearest vs bilinear is obvious |
| `assets/nova_uv.ppm` | 64x64 uv grid (red ramps with u, green with v, white lines) - shows the uv layout and wrapping |

Any tool that writes PPM works as a texture converter, e.g.
`python3 tools/ppm_to_png.py` in reverse, ImageMagick (`magick in.png out.ppm`),
or GIMP's PPM export. No image library is linked into the renderer.

## Demo

```bash
./build/bin/nova_texture_demo --width 640 --height 480 --prefix texture
#   texture_nearest.ppm   3x3 repeated checker, nearest filtering
#   texture_bilinear.ppm  the same with bilinear filtering
#   texture_uvgrid.ppm    uv grid, so interpolation/wrapping is readable
#   texture_lit.ppm       texture as albedo, lit (Phase 4)
```

The demo loads the files above when they are present and falls back to the same
images generated procedurally, so it always runs.

## Verified behaviour

`tests/test_texture.cpp` covers: texel access with wrapping, nearest sampling at
texel centres, bilinear being exact at texel centres and the exact average
between four texels, repeat vs clamp, procedural constructors, empty-texture
safety, PPM round-trip (memory and filesystem), P6/P3 decoding, rejection of junk
and truncated input, a textured quad rendering each texel into its own screen
quadrant, and bilinear producing blended pixels where nearest produces none.

## Not here yet

* **Mipmaps** - the task list keeps mip chain generation + trilinear selection for
  later; the sampler has no mip bias parameter yet.
* **Alpha blending / transparency** - sampling returns alpha, but the framebuffer
  write is opaque.
* **sRGB / linear light** - texels are used as stored, matching the display-referred
  framebuffer.
* **PNG/JPEG** - only PPM is decoded; a real decoder can be added behind
  `loadPpm`'s interface.
