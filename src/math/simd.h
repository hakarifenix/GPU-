// NOVA GPU - Software GPU
// src/math/simd.h - optional SIMD helpers.
//
// Phase 5 rule: only use SIMD where profiling shows it pays. The vertex stage is
// the one place where a wider unit maps cleanly onto the work - four independent
// vertices go through the same matrices - so that is the only SIMD kernel NOVA
// ships. Everything else stays scalar, and the benchmark's --compare mode
// measures what this is actually worth on the host (see docs/performance.md).
//
// The kernel is 4-wide: SSE2 is baseline on x86-64, and the rest of the code
// stays portable because every call site has a scalar fallback.
#pragma once

#if !defined(NOVA_NO_SIMD) && (defined(__SSE2__) || defined(_M_X64) || \
                              (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define NOVA_HAS_SSE2 1
#include <emmintrin.h>
#endif

namespace nova::math {

#if defined(NOVA_HAS_SSE2)

// Applies `m` to four points/directions in structure-of-arrays form:
//   out[i] = m * (x[i], y[i], z[i], w)
// The matrix elements are broadcast once and reused across the four lanes.
inline void transform4(const Mat4& m, __m128 x, __m128 y, __m128 z, __m128 w, __m128& outX,
                       __m128& outY, __m128& outZ, __m128& outW) {
    const __m128 r0 = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][0]), x), _mm_mul_ps(_mm_set1_ps(m.m[0][1]), y)),
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[0][2]), z), _mm_mul_ps(_mm_set1_ps(m.m[0][3]), w)));
    const __m128 r1 = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][0]), x), _mm_mul_ps(_mm_set1_ps(m.m[1][1]), y)),
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[1][2]), z), _mm_mul_ps(_mm_set1_ps(m.m[1][3]), w)));
    const __m128 r2 = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][0]), x), _mm_mul_ps(_mm_set1_ps(m.m[2][1]), y)),
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[2][2]), z), _mm_mul_ps(_mm_set1_ps(m.m[2][3]), w)));
    const __m128 r3 = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[3][0]), x), _mm_mul_ps(_mm_set1_ps(m.m[3][1]), y)),
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m.m[3][2]), z), _mm_mul_ps(_mm_set1_ps(m.m[3][3]), w)));
    outX = r0;
    outY = r1;
    outZ = r2;
    outW = r3;
}

// Transforms four direction vectors (implicit w = 0) and ignores the result's w.
inline void transformDirections4(const Mat4& m, __m128 x, __m128 y, __m128 z, __m128& outX,
                                 __m128& outY, __m128& outZ) {
    __m128 ignore;
    transform4(m, x, y, z, _mm_setzero_ps(), outX, outY, outZ, ignore);
}

#endif  // NOVA_HAS_SSE2

}  // namespace nova::math
