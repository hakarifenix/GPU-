#include "texture/image_io.h"

#include <cstdio>
#include <vector>

namespace nova::texture {
namespace {

// Parses one unsigned integer, skipping whitespace and '#' comments.
bool readUnsigned(const uint8_t* data, size_t size, size_t& pos, uint32_t& out) {
    while (pos < size) {
        const uint8_t c = data[pos];
        if (c == '#') {
            while (pos < size && data[pos] != '\n') ++pos;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++pos;
        } else {
            break;
        }
    }
    if (pos >= size || data[pos] < '0' || data[pos] > '9') return false;

    uint32_t value = 0;
    while (pos < size && data[pos] >= '0' && data[pos] <= '9') {
        value = value * 10u + static_cast<uint32_t>(data[pos] - '0');
        if (value > 100000u) return false;
        ++pos;
    }
    out = value;
    return true;
}

}  // namespace

Texture decodePpm(const uint8_t* data, size_t size, std::string* error) {
    if (data == nullptr || size < 2) {
        if (error) *error = "empty input";
        return Texture();
    }
    const bool binary = data[0] == 'P' && data[1] == '6';
    const bool ascii = data[0] == 'P' && data[1] == '3';
    if (!binary && !ascii) {
        if (error) *error = "not a PPM (expected P6 or P3)";
        return Texture();
    }

    size_t pos = 2;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t maxValue = 0;
    if (!readUnsigned(data, size, pos, width) || !readUnsigned(data, size, pos, height) ||
        !readUnsigned(data, size, pos, maxValue)) {
        if (error) *error = "malformed header";
        return Texture();
    }
    if (width == 0 || height == 0 || maxValue == 0 || maxValue > 255) {
        if (error) *error = "unsupported dimensions or max value";
        return Texture();
    }

    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<core::Color> pixels(pixelCount);
    const float scale = 255.0f / static_cast<float>(maxValue);

    if (binary) {
        // Exactly one whitespace character separates the header from the data.
        if (pos >= size) {
            if (error) *error = "missing pixel data";
            return Texture();
        }
        ++pos;
        if (size - pos < pixelCount * 3) {
            if (error) *error = "truncated pixel data";
            return Texture();
        }
        for (size_t i = 0; i < pixelCount; ++i) {
            const uint8_t r = data[pos + i * 3 + 0];
            const uint8_t g = data[pos + i * 3 + 1];
            const uint8_t b = data[pos + i * 3 + 2];
            pixels[i] = core::Color{static_cast<uint8_t>(r * scale + 0.5f),
                                    static_cast<uint8_t>(g * scale + 0.5f),
                                    static_cast<uint8_t>(b * scale + 0.5f), 255};
        }
    } else {
        for (size_t i = 0; i < pixelCount; ++i) {
            uint32_t r = 0;
            uint32_t g = 0;
            uint32_t b = 0;
            if (!readUnsigned(data, size, pos, r) || !readUnsigned(data, size, pos, g) ||
                !readUnsigned(data, size, pos, b)) {
                if (error) *error = "truncated ASCII pixel data";
                return Texture();
            }
            pixels[i] = core::Color{static_cast<uint8_t>(r * scale + 0.5f),
                                    static_cast<uint8_t>(g * scale + 0.5f),
                                    static_cast<uint8_t>(b * scale + 0.5f), 255};
        }
    }

    return Texture(static_cast<int>(width), static_cast<int>(height), std::move(pixels));
}

Texture loadPpm(const std::string& path) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return Texture();

    std::vector<uint8_t> bytes;
    uint8_t buffer[4096];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        bytes.insert(bytes.end(), buffer, buffer + read);
    }
    std::fclose(file);

    return decodePpm(bytes.data(), bytes.size(), nullptr);
}

std::string encodePpm(const Texture& texture) {
    if (texture.empty()) return std::string();

    char header[64];
    const int written = std::snprintf(header, sizeof(header), "P6\n%d %d\n255\n", texture.width(),
                                      texture.height());
    std::string out(header, static_cast<size_t>(written));
    out.reserve(out.size() + texture.pixels().size() * 3);
    for (const core::Color& c : texture.pixels()) {
        out.push_back(static_cast<char>(c.r));
        out.push_back(static_cast<char>(c.g));
        out.push_back(static_cast<char>(c.b));
    }
    return out;
}

bool savePpm(const std::string& path, const Texture& texture) {
    const std::string bytes = encodePpm(texture);
    if (bytes.empty()) return false;

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) return false;
    const size_t written = std::fwrite(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    return written == bytes.size();
}

}  // namespace nova::texture
