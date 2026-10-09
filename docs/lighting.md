# NOVA GPU lighting (Phase 4)

One CPU-friendly model covers the whole phase - ambient + directional diffuse,
with an optional Blinn-Phong highlight:

```
ambient  = albedo * ambient.color * (ambient.intensity * material.ambient)
diffuse  = albedo * light.color   * (light.intensity * material.diffuse * max(N.L, 0))
specular =          light.color   * (light.intensity * material.specular * pow(N.H, shininess))
```

* `N` is the interpolated world normal (normalized per fragment), `L` points
  towards the light (`-light.direction`) and `H = normalize(L + V)` is the
  Blinn-Phong halfway vector with `V` the unit vector towards the camera.
* `material.specular == 0` gives the Lambert model, so both Phase 4 shaders share
  the same code path.
* The result is not clamped inside the shader; the framebuffer clamps when it
  writes 8-bit color, exactly like an unclamped float GPU target.

## Files

| File | Contents |
| --- | --- |
| `src/lighting/light.h` | `AmbientLight`, `DirectionalLight`, `Material`, `Surface` |
| `src/lighting/lighting.h/.cpp` | `shade(...)`, the whole model |
| `src/shader/lambert_shader.h` | `LambertFragmentShader` - albedo = vertex color, no highlight |
| `src/shader/phong_shader.h` | `PhongFragmentShader` - material + optional albedo texture + highlight |
| `src/shader/uniforms.h` | `ShaderUniforms::lights` (`ambient`, `directional`) |

## Using it

```cpp
lighting::AmbientLight ambient;
ambient.color = {0.85f, 0.9f, 1.0f};
ambient.intensity = 0.2f;
device.setAmbient(ambient);

lighting::DirectionalLight sun;
sun.direction = math::normalize({-0.45f, -0.75f, -0.48f});  // travelling this way
sun.color = {1.0f, 0.97f, 0.92f};
sun.intensity = 1.0f;
device.setLight(sun);

lighting::Material material;
material.albedo  = {0.8f, 0.85f, 0.9f};
material.specular = 0.4f;     // 0 => Lambert
material.shininess = 48.0f;

shader::PhongFragmentShader shader(material);
shader.setAlbedoTexture(&checker);   // optional; albedo *= texel * vertex color
device.draw(sphere, math::Mat4::identity(), cam.viewProjection(), shader);
```

`setCameraPosition` supplies the `V` direction (`Surface::viewDirection`), and
lights are captured into the uniforms block when a draw command is recorded.

## Cost, and one measured optimisation

Per fragment the diffuse path is one normalize, two dots and a handful of
multiplies - the same order as the color shader. The highlight adds a `pow()`,
which dominated the whole shader: on a 480x360 sphere, `lighting_phong` measured
**43 ms/frame** against 4 ms for the same scene with `specular = 0`.

`shade()` therefore skips the highlight when it cannot reach one 8-bit step
(`shininess >= 8` and `dot(N, H) < 0.5`, since `0.5^8 = 0.0039 < 1/255`). The
same scene then measured **5.7 ms/frame** - the difference is invisible and is
pinned by `Lighting.SpecularCutoffStaysBelowOneLsb`, which checks that the
skipped term stays below 1/255 and that a broad highlight (`shininess = 4`) is
still evaluated.

## Demo

```bash
./build/bin/nova_lighting_demo --width 640 --height 480 --prefix lighting
#   lighting_lambert.ppm   ambient + directional diffuse
#   lighting_phong.ppm     adds the specular highlight
#   lighting_backlit.ppm   light behind the objects: ambient term only
#   lighting_colored.ppm   blue ambient + orange key light
```

The scene is a smooth-normal sphere (`scene::makeSphere`), a cube and a floor, so
the diffuse falloff and the highlight are visible as gradients rather than as six
flat shades.

## Verified behaviour

`tests/test_lighting.cpp` covers: ambient scaling/tinting, diffuse full-on facing
the light and zero facing away, cosine falloff at 60 degrees, linearity in
intensity/albedo, the specular peak and its sharp falloff, specular suppression on
back-facing surfaces, a degenerate (zero) normal staying NaN-free, a shaded
sphere producing a smooth gradient that is brighter towards the light, and Phong
producing a *localised* highlight that a matte material does not have.

## Not here yet

* Several lights / light arrays (the uniforms block holds one ambient + one
  directional light; `Lights` is the place to grow).
* Point/spot lights with distance attenuation (`Surface::position` is already
  carried for this).
* Shadows, normal mapping, physically based materials, gamma-correct blending.
