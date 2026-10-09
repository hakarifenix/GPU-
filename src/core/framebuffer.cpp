#include "core/framebuffer.h"

#include <stdexcept>
#include <string>

namespace nova::core {

Framebuffer::Framebuffer(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Framebuffer: width and height must be positive (got " +
                                    std::to_string(width) + "x" + std::to_string(height) + ")");
    }
    color_.assign(pixelCount(), Color{});
    depth_.assign(pixelCount(), 1.0f);
}

void Framebuffer::clearColor(const Color& c) {
    for (Color& pixel : color_) pixel = c;
}

void Framebuffer::clearDepth(float depth) {
    for (float& d : depth_) d = depth;
}

void Framebuffer::clear(const Color& c, float depth) {
    clearColor(c);
    clearDepth(depth);
}

}  // namespace nova::core
