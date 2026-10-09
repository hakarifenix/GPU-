// NOVA GPU - Software GPU
// src/texture/image_io.h - image files in and out, without any dependency.
//
// NOVA deliberately ships no image library: the loader below reads the PPM
// formats (P6 binary and P3 ASCII), which is enough for tests, demos and any
// texture a user wants to convert with a one-line tool. PNG/JPEG decoding can be
// added later behind the same interface without touching the renderer.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "texture/texture.h"

namespace nova::texture {

// Loads a PPM file. Returns an empty texture on failure.
Texture loadPpm(const std::string& path);

// Decodes PPM bytes already in memory (no filesystem access).
Texture decodePpm(const uint8_t* data, size_t size, std::string* error = nullptr);

// Writes a texture as binary P6 PPM. Returns false on failure.
bool savePpm(const std::string& path, const Texture& texture);

// Encodes a texture as P6 PPM bytes.
std::string encodePpm(const Texture& texture);

}  // namespace nova::texture
