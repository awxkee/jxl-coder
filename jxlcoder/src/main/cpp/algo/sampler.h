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
static inline T SimpleCubic(T t, T A, T B, T C, T D) {
    T duplet = t * t;
    T triplet = duplet * t;
    T a = -A / T(2.0) + (T(3.0) * B) / T(2.0) - (T(3.0) * C) / T(2.0) + D / T(2.0);
    T b = A - (T(5.0) * B) / T(2.0) + T(2.0) * C - D / T(2.0);
    T c = -A / T(2.0) + C / T(2.0);
    T d = B;
    return a * triplet * T(3.0) + b * duplet + c * t + d;
}

template<typename T>
static inline T CubicHermite(T d, T p0, T p1, T p2, T p3) {
    constexpr T C = T(0.0);
    constexpr T B = T(0.0);
    T duplet = d * d;
    T triplet = duplet * d;
    T firstRow = ((T(-1 / 6.0) * B - C) * p0 + (T(-1.5) * B - C + T(2.0)) * p1 +
                  (T(1.5) * B + C - T(2.0)) * p2 + (T(1 / 6.0) * B + C) * p3) * triplet;
    T secondRow = ((T(0.5) * B + 2 * C) * p0 + (T(2.0) * B + C - T(3.0)) * p1 +
                   (T(-2.5) * B - T(2.0) * C + T(3.0)) * p2 - C * p3) * duplet;
    T thirdRow = ((T(-0.5) * B - C) * p0 + (T(0.5) * B + C) * p2) * d;
    T fourthRow =
            (T(1.0 / 6.0) * B) * p0 + (T(-1.0 / 3.0) * B + T(1)) * p1 + (T(1.0 / 6.0) * B) * p2;
    return firstRow + secondRow + thirdRow + fourthRow;
}

template<typename T>
static inline T CubicBSpline(T d, T p0, T p1, T p2, T p3) {
    constexpr T C = T(0.0);
    constexpr T B = T(1.0);
    T duplet = d * d;
    T triplet = duplet * d;
    T firstRow = ((T(-1 / 6.0) * B - C) * p0 + (T(-1.5) * B - C + T(2.0)) * p1 +
                  (T(1.5) * B + C - T(2.0)) * p2 + (T(1 / 6.0) * B + C) * p3) * triplet;
    T secondRow = ((T(0.5) * B + 2 * C) * p0 + (T(2.0) * B + C - T(3.0)) * p1 +
                   (T(-2.5) * B - T(2.0) * C + T(3.0)) * p2 - C * p3) * duplet;
    T thirdRow = ((T(-0.5) * B - C) * p0 + (T(0.5) * B + C) * p2) * d;
    T fourthRow =
            (T(1.0 / 6.0) * B) * p0 + (T(-1.0 / 3.0) * B + T(1)) * p1 + (T(1.0 / 6.0) * B) * p2;
    return firstRow + secondRow + thirdRow + fourthRow;
}

template<typename T>
static inline T MitchellNetravali(T d, T p0, T p1, T p2, T p3) {
    constexpr T C = T(1.0 / 3.0);
    constexpr T B = T(1.0 / 3.0);
    T duplet = d * d;
    T triplet = duplet * d;
    T firstRow = ((T(-1 / 6.0) * B - C) * p0 + (T(-1.5) * B - C + T(2.0)) * p1 +
                  (T(1.5) * B + C - T(2.0)) * p2 + (T(1 / 6.0) * B + C) * p3) * triplet;
    T secondRow = ((T(0.5) * B + 2 * C) * p0 + (T(2.0) * B + C - T(3.0)) * p1 +
                   (T(-2.5) * B - T(2.0) * C + T(3.0)) * p2 - C * p3) * duplet;
    T thirdRow = ((T(-0.5) * B - C) * p0 + (T(0.5) * B + C) * p2) * d;
    T fourthRow =
            (T(1.0 / 6.0) * B) * p0 + (T(-1.0 / 3.0) * B + T(1)) * p1 + (T(1.0 / 6.0) * B) * p2;
    return firstRow + secondRow + thirdRow + fourthRow;
}

template<typename T>
static inline T sinc(T x) {
    if (x == 0.0) {
        return T(1.0);
    } else {
        return fastSin2(x) / x;
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
static inline T CatmullRom(T x, T p0, T p1, T p2, T p3) {
    if (x < 0 || x > 1) {
        return T(0);
    }
    T s1 = x * (-p0 + 3 * p1 - 3 * p2 + p3);
    T s2 = (T(2.0) * p0 - T(5.0) * p1 + 4 * p2 - p3);
    T s3 = (s1 + s2) * x;
    T s4 = (-p0 + p2);
    T s5 = (s3 + s4) * x * T(0.5);

    return s5 + p1;
}

template<typename T>
inline T HannWindow(const T n, const T length) {
    const float size = length * 2;
    if (abs(n) <= size) {
        float v = sin(M_PI * n / size);
        return v * v;
    }
    return T(0);
}

#endif //JXLCODER_SAMPLER_H
