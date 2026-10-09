# src/texture - reserved for Phase 3

Empty in Phase 2: the pipeline already carries and perspective-correctly
interpolates texture coordinates (`core::Vertex::uv` -> `shader::Varying::uv` ->
`pipeline::ScreenTriangle::varyingOverW` -> `shader::FragmentInput::varying.uv`),
and `shader::CheckerFragmentShader` proves they arrive intact at the fragment
stage, but nothing samples an actual image yet.

Phase 3 adds texture storage, loading and nearest/bilinear sampling here, plus
mipmaps later. Keep sampling out of the rasterizer: it belongs to the fragment
stage so that the same texture object works for every shader.
