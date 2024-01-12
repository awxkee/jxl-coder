//
// Created by Radzivon Bartoshyk on 11/01/2024.
//

#ifndef JXLCODER_SAMPLER_H
#define JXLCODER_SAMPLER_H

#include "../half.hpp"
#include <algorithm>

using namespace std;
using namespace half_float;

// P Found using maxima
//
// y(x) := 4 * x * (%pi-x) / (%pi^2) ;
// z(x) := (1-p)*y(x) + p * y(x)^2;
// e(x) := z(x) - sin(x);
// solve( diff( integrate( e(x)^2, x, 0, %pi/2 ), p ) = 0, p ),numer;
//
// [p = .2248391013559941]
template<typename T>
static inline T fastSin1(T x) {
    constexpr T A = T(4.0) / (T(M_PI) * T(M_PI));
    constexpr T P = 0.2248391013559941;
    T y = A * x * (T(M_PI) - x);
    return y * ((1 - P) + y * P);
}

// P and Q found using maxima
//
// y(x) := 4 * x * (%pi-x) / (%pi^2) ;
// zz(x) := (1-p-q)*y(x) + p * y(x)^2 + q * y(x)^3
// ee(x) := zz(x) - sin(x)
// solve( [ integrate( diff(ee(x)^2, p ), x, 0, %pi/2 ) = 0, integrate( diff(ee(x)^2,q), x, 0, %pi/2 ) = 0 ] , [p,q] ),numer;
//
// [[p = .1952403377008734, q = .01915214119105392]]
template<typename T>
static inline T fastSin2(T x) {
    constexpr T A = T(4.0) / (T(M_PI) * T(M_PI));
    constexpr T P = 0.1952403377008734;
    constexpr T Q = 0.01915214119105392;

    T y = A * x * (T(M_PI) - x);

    return y * ((1 - P - Q) + y * (P + y * Q));
}

static inline half_float::half castU16(uint16_t t) {
    half_float::half result;
    result.data_ = t;
    return result;
}

template<typename D, typename T>
static inline D PromoteTo(T t, float maxColors) {
    D result = static_cast<D>((float) t / maxColors);
    return result;
}

template<typename D, typename T>
static inline D DemoteTo(T t, float maxColors) {
    return (D) clamp(((float) t * (float) maxColors), 0.0f, (float) maxColors);
}

template<typename T>
static inline float BCSpline(T x, const T B, const T C) {
    if (x < 0.0f) x = -x;

    const T dp = x * x;
    const T tp = dp * x;

    if (x < 1.0f)
        return ((12 - 9 * B - 6 * C) * tp + (-18 + 12 * B + 6 * C) * dp + (6 - 2 * B)) *
               (T(1) / T(6));
    else if (x < 2.0f)
        return ((-B - 6 * C) * tp + (6 * B + 30 * C) * dp + (-12 * B - 48 * C) * x +
                (8 * B + 24 * C)) * (T(1) / T(6));

    return (0.0f);
}

template<typename T>
static inline T SimpleCubic(T x) {
    if (x < 0.0f) x = -x;

    if (x < 1.0f)
        return (4.0f + x * x * (3.0f * x - 6.0f)) / 6.0f;
    else if (x < 2.0f)
        return (8.0f + x * (-12.0f + x * (6.0f - x))) / 6.0f;

    return (0.0f);
}

template<typename T>
static inline T CubicHermite(T x) {
    constexpr T C = T(0.0);
    constexpr T B = T(0.0);
    return BCSpline(x, B, C);
}

template<typename T>
static inline float BSpline(T x) {
    constexpr T C = T(0.0);
    constexpr T B = T(1.0);
    return BCSpline(x, B, C);
}

template<typename T>
static inline float MitchellNetravalli(T x) {
    constexpr T B = 1.0f / 3.0f;
    constexpr T C = 1.0f / 3.0f;
    return BCSpline(x, B, C);
}

template<typename T>
static inline T sinc(T x) {
    if (x == 0.0) {
        return T(1.0);
    } else {
        return sin(x) / x;
    }
}

template<typename T>
static inline T LanczosWindow(T x, const T a) {
    if (abs(x) < a) {
        return sinc(T(M_PI) * x) * sinc(T(M_PI) * x / a);
    }
    return T(0.0);
}

template<typename T>
static inline T fastCos(T x) {
    constexpr T C0 = 0.99940307;
    constexpr T C1 = -0.49558072;
    constexpr T C2 = 0.03679168;
    constexpr T C3 = -0.00434102;

    while (x < -2 * M_PI) {
        x += 2.0 * M_PI;
    }
    while (x > 2 * M_PI) {
        x -= 2.0 * M_PI;
    }

    // Calculate cos(x) using Chebyshev polynomial approximation
    T x2 = x * x;
    T result = C0 + x2 * (C1 + x2 * (C2 + x2 * C3));
    return result;
}

template<typename T>
static inline T CatmullRom(T x) {
    return BCSpline(x, 0.0f, 0.5f);
}

template<typename T>
static inline T HannWindow(const T n, const T length) {
    const T size = length * 2;
    const T part = M_PI / size;
    if (abs(n) > length) {
        return 0;
    }
    T r = cos(n * part);
    r = r * r;
    return r / size;
}

template<typename T>
static inline T blerp(T c00, T c10, T c01, T c11, T tx, T ty) {
    return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
}

#endif //JXLCODER_SAMPLER_H
