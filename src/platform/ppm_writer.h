// NOVA GPU - Software GPU
// src/platform/ppm_writer.h - framebuffer -> PPM (P6) image file.
//
// PPM is the Phase 1/2 "display output": a dependency-free, lossless RGB dump
// of the framebuffer. A windowed presenter can be added later without touching
// the renderer; see docs/output.md.
#pragma once

#include <string>

#include "core/framebuffer.h"

namespace nova::platform {

// Writes "P6" binary PPM. Returns false (and leaves no partial file) on failure.
bool writePpm(const std::string& path, const core::Framebuffer& framebuffer);

// Encodes the framebuffer as PPM bytes (used by tests to avoid the filesystem).
std::string encodePpm(const core::Framebuffer& framebuffer);

}  // namespace nova::platform
