#include "platform/console.h"

#include <cstdarg>
#include <cstdio>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace nova::platform {

void enableAnsiColors() {
#if defined(_WIN32)
    // Windows 10 1511+ : opt this console into virtual terminal processing so
    // the ANSI escapes below work. Ignored if the call fails.
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode) == 0) return;
    SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

void resetColor() {
    std::fputs("\x1b[0m", stdout);
}

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(stdout, format, args);
    va_end(args);
}

std::string formatDouble(double value, int decimals) {
    if (decimals < 0) decimals = 0;
    std::string out = "%." + std::to_string(decimals) + "f";
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), out.c_str(), value);
    return buffer;
}

std::string formatThousands(unsigned long long value) {
    const std::string digits = std::to_string(value);
    std::string out;
    out.reserve(digits.size() + digits.size() / 3);
    for (size_t i = 0; i < digits.size(); ++i) {
        // Separate when the number of remaining digits is a multiple of three,
        // which groups from the right without any unsigned-underflow hazards.
        if (i != 0 && (digits.size() - i) % 3 == 0) out.push_back(',');
        out.push_back(digits[i]);
    }
    return out;
}

}  // namespace nova::platform
