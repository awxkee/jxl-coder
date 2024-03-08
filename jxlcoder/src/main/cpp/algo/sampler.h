//
// Created by Radzivon Bartoshyk on 11/01/2024.
//

#ifndef JXLCODER_SAMPLER_H
#define JXLCODER_SAMPLER_H

#include "conversion/half.hpp"
#include <algorithm>

using namespace std;
using namespace half_float;

static inline half_float::half castU16(uint16_t t) {
  half_float::half result;
  result.data_ = t;
  return result;
}

template<typename D, typename T>
static inline D PromoteTo(T t, float maxColors) {
  D result = static_cast<D>(static_cast<float>(t) / maxColors);
  return result;
}

template<typename D, typename T>
static inline D DemoteTo(T t, float maxColors) {
  return (D) std::clamp((static_cast<float>(t) * static_cast<float>(maxColors)), 0.0f, static_cast<float>(maxColors));
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
static inline T BiCubicSpline(T x, const T a = -0.5) {
  const T modulo = std::abs(x);
  if (modulo >= 2) {
    return 0;
  }
  const T doubled = modulo * modulo;
  const T triplet = doubled * modulo;
  if (modulo <= 1) {
    return (a + T(2.0)) * triplet - (a + T(3.0)) * doubled + T(1.0);
  }
  return a * triplet - T(5.0) * a * doubled + T(8.0) * a * modulo - T(4.0) * a;
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
    return std::sinf(x) / x;
  }
}

template<typename T>
static inline T LanczosWindow(T x, const T a) {
  if (std::abs(x) < a) {
    return sinc(static_cast<T>(M_PI) * x) * sinc(static_cast<float>(M_PI) * x / a);
  }
  return static_cast<T>(0.0);
}

template<typename T>
static inline T CatmullRom(T x) {
  return BCSpline(x, 0.0f, 0.5f);
}

template<typename T>
static inline T Hann(const T n, const T length) {
  const T size = length * 2;
  const T part = M_PI / size;
  if (abs(n) > length) {
    return 0;
  }
  T r = std::cosf(n * part);
  r = r * r;
  return r / size;
}

template<typename T>
static inline T blerp(T c00, T c10, T c01, T c11, T tx, T ty) {
  return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
}

#endif //JXLCODER_SAMPLER_H
