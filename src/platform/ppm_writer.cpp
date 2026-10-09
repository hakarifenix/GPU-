#include "platform/ppm_writer.h"

#include <cstdio>
#include <vector>

namespace nova::platform {

std::string encodePpm(const core::Framebuffer& framebuffer) {
    std::string out;
    const int width = framebuffer.width();
    const int height = framebuffer.height();

    char header[64];
    const int headerLength = std::snprintf(header, sizeof(header), "P6\n%d %d\n255\n", width, height);
    out.append(header, static_cast<size_t>(headerLength));

    const size_t pixels = framebuffer.pixelCount();
    out.resize(out.size() + pixels * 3);
    char* dst = &out[out.size() - pixels * 3];

    const core::Color* color = framebuffer.colorData();
    for (size_t i = 0; i < pixels; ++i) {
        *dst++ = static_cast<char>(color[i].r);
        *dst++ = static_cast<char>(color[i].g);
        *dst++ = static_cast<char>(color[i].b);
    }
    return out;
}

bool writePpm(const std::string& path, const core::Framebuffer& framebuffer) {
    const std::string bytes = encodePpm(framebuffer);

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) return false;

    const size_t written = std::fwrite(bytes.data(), 1, bytes.size(), file);
    const bool ok = (written == bytes.size()) && (std::fclose(file) == 0);
    if (!ok) std::remove(path.c_str());
    return ok;
}

}  // namespace nova::platform
