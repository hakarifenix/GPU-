#include "pipeline/clipper.h"

#include <array>

namespace nova::pipeline {

int clipPolygonAgainstPlane(const ClipVertex* in, int inCount, const math::Vec4& plane,
                            ClipVertex* out, int maxOut) {
    if (inCount <= 0) return 0;

    int outCount = 0;
    for (int i = 0; i < inCount; ++i) {
        const ClipVertex& current = in[i];
        const ClipVertex& next = in[(i + 1) % inCount];

        const float dCurrent = math::dot(plane, current.clip);
        const float dNext = math::dot(plane, next.clip);

        const bool currentInside = dCurrent >= 0.0f;
        const bool nextInside = dNext >= 0.0f;

        if (currentInside) {
            if (outCount < maxOut) out[outCount++] = current;
        }
        // Crossing the plane -> emit the intersection point.
        if (currentInside != nextInside) {
            const float t = dCurrent / (dCurrent - dNext);
            if (outCount < maxOut) out[outCount++] = lerpClipVertex(current, next, t);
        }
    }
    return outCount;
}

int clipTriangle(const ClipVertex in[3], bool enableNearClip, ClipVertex out[4]) {
    std::array<ClipVertex, 8> a{};
    std::array<ClipVertex, 8> b{};

    a[0] = in[0];
    a[1] = in[1];
    a[2] = in[2];
    int count = 3;

    if (enableNearClip) {
        count = clipPolygonAgainstPlane(a.data(), count, nearPlane(), b.data(), 8);
        if (count < 3) return 0;
        a = b;
    }

    count = clipPolygonAgainstPlane(a.data(), count, positiveWPlane(), b.data(), 8);
    if (count < 3) return 0;

    if (count > 4) count = 4;  // cannot happen with two planes on a triangle
    for (int i = 0; i < count; ++i) out[i] = b[static_cast<size_t>(i)];
    return count;
}

}  // namespace nova::pipeline
