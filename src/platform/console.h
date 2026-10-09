// NOVA GPU - Software GPU
// src/platform/console.h - tiny console helpers used by examples/benchmarks.
//
// Keeps terminal handling (ANSI escape sequences, Windows virtual terminal
// mode) out of the renderer and the example code.
#pragma once

#include <string>

namespace nova::platform {

// Enables ANSI escape handling on Windows 10+ consoles; a no-op elsewhere and
// safe to call on every platform. Never fatal if the console does not support
// it (output is simply uncolored).
void enableAnsiColors();

// Resets text color. Harmless if colors were never enabled.
void resetColor();

// Formatted stdout helpers with optional color.
void printf(const char* format, ...);

// Renders a double with a fixed number of decimals into a string, without
// depending on locale-sensitive stream formatting.
std::string formatDouble(double value, int decimals);

// "1234567" -> "1,234,567" (helps read large triangle/fragment counts).
std::string formatThousands(unsigned long long value);

}  // namespace nova::platform
