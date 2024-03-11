/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 05/09/2023
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "Rgb1010102.h"
#include <vector>
#include <algorithm>
#include "conversion/HalfFloats.h"
#include <thread>
#include "concurrency.hpp"

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "conversion/Rgb1010102.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "imagebit/attenuate-inl.h"
#include "algo/math-inl.h"

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

using hwy::HWY_NAMESPACE::FixedTag;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::Vec;
using hwy::HWY_NAMESPACE::Set;
using hwy::HWY_NAMESPACE::Zero;
using hwy::HWY_NAMESPACE::LoadU;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::And;
using hwy::HWY_NAMESPACE::Add;
using hwy::HWY_NAMESPACE::BitCast;
using hwy::HWY_NAMESPACE::ShiftRight;
using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::ExtractLane;
using hwy::HWY_NAMESPACE::DemoteTo;
using hwy::HWY_NAMESPACE::ConvertTo;
using hwy::HWY_NAMESPACE::PromoteTo;
using hwy::HWY_NAMESPACE::LoadInterleaved4;
using hwy::HWY_NAMESPACE::Or;
using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::StoreU;
using hwy::HWY_NAMESPACE::Round;
using hwy::HWY_NAMESPACE::ClampRound;
using hwy::float16_t;

void
F16ToRGBA1010102HWYRow(const uint16_t *JXL_RESTRICT data, uint32_t *JXL_RESTRICT dst,
                       const uint32_t width,
                       const int *permuteMap) {
  float range10 = std::powf(2.f, 10.f) - 1.f;
  const FixedTag<float, 4> df;
  const FixedTag<float16_t, 8> df16;
  const FixedTag<uint16_t, 8> du16x8;
  const Rebind<int32_t, FixedTag<float, 4>> di32;
  const FixedTag<uint32_t, 4> du;
  using V = Vec<decltype(df)>;
  using V16 = Vec<decltype(df16)>;
  using VU16x8 = Vec<decltype(du16x8)>;
  using VU = Vec<decltype(du)>;
  const auto vRange10 = Set(df, range10);
  const auto zeros = Zero(df);
  const auto alphaMax = Set(df, 3.0);

  const Rebind<float16_t, FixedTag<uint16_t, 8>> rbfu16;
  const Rebind<float, FixedTag<float16_t, 4>> rbf32;

  const int permute0Value = permuteMap[0];
  const int permute1Value = permuteMap[1];
  const int permute2Value = permuteMap[2];
  const int permute3Value = permuteMap[3];

  uint32_t x = 0;
  auto dst32 = reinterpret_cast<uint32_t *>(dst);
  int pixels = 8;
  for (; x + pixels < width; x += pixels) {
    VU16x8 upixels1;
    VU16x8 upixels2;
    VU16x8 upixels3;
    VU16x8 upixels4;
    LoadInterleaved4(du16x8, reinterpret_cast<const uint16_t *>(data),
                     upixels1,
                     upixels2,
                     upixels3, upixels4);
    V16 fpixels1 = BitCast(rbfu16, upixels1);
    V16 fpixels2 = BitCast(rbfu16, upixels2);
    V16 fpixels3 = BitCast(rbfu16, upixels3);
    V16 fpixels4 = BitCast(rbfu16, upixels4);
    V pixelsLow1 = ClampRound(rbf32, Mul(PromoteLowerTo(rbf32, fpixels1), vRange10), zeros,
                              vRange10);
    V pixelsLow2 = ClampRound(rbf32, Mul(PromoteLowerTo(rbf32, fpixels2), vRange10), zeros,
                              vRange10);
    V pixelsLow3 = ClampRound(rbf32, Mul(PromoteLowerTo(rbf32, fpixels3), vRange10), zeros,
                              vRange10);
    V pixelsLow4 = ClampRound(rbf32, Mul(PromoteLowerTo(rbf32, fpixels4), alphaMax), zeros,
                              alphaMax);
    VU pixelsuLow1 = BitCast(du, ConvertTo(di32, pixelsLow1));
    VU pixelsuLow2 = BitCast(du, ConvertTo(di32, pixelsLow2));
    VU pixelsuLow3 = BitCast(du, ConvertTo(di32, pixelsLow3));
    VU pixelsuLow4 = BitCast(du, ConvertTo(di32, pixelsLow4));

    V pixelsHigh1 = ClampRound(rbf32, Mul(PromoteUpperTo(rbf32, fpixels1), vRange10), zeros,
                               vRange10);
    V pixelsHigh2 = ClampRound(rbf32, Mul(PromoteUpperTo(rbf32, fpixels2), vRange10), zeros,
                               vRange10);
    V pixelsHigh3 = ClampRound(rbf32, Mul(PromoteUpperTo(rbf32, fpixels3), vRange10), zeros,
                               vRange10);
    V pixelsHigh4 = ClampRound(rbf32, Mul(PromoteUpperTo(rbf32, fpixels4), alphaMax), zeros,
                               alphaMax);
    VU pixelsuHigh1 = BitCast(du, ConvertTo(di32, pixelsHigh1));
    VU pixelsuHigh2 = BitCast(du, ConvertTo(di32, pixelsHigh2));
    VU pixelsuHigh3 = BitCast(du, ConvertTo(di32, pixelsHigh3));
    VU pixelsuHigh4 = BitCast(du, ConvertTo(di32, pixelsHigh4));

    VU pixelsHighStore[4] = {pixelsuHigh1, pixelsuHigh2, pixelsuHigh3, pixelsuHigh4};
    VU AV = pixelsHighStore[permute0Value];
    VU RV = pixelsHighStore[permute1Value];
    VU GV = pixelsHighStore[permute2Value];
    VU BV = pixelsHighStore[permute3Value];
    VU upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
    VU lower = Or(ShiftLeft<10>(GV), BV);
    VU final = Or(upper, lower);
    StoreU(final, du, dst32 + 4);

    VU pixelsLowStore[4] = {pixelsuLow1, pixelsuLow2, pixelsuLow3, pixelsuLow4};
    AV = pixelsLowStore[permute0Value];
    RV = pixelsLowStore[permute1Value];
    GV = pixelsLowStore[permute2Value];
    BV = pixelsLowStore[permute3Value];
    upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
    lower = Or(ShiftLeft<10>(GV), BV);
    final = Or(upper, lower);
    StoreU(final, du, dst32);

    data += pixels * 4;
    dst32 += pixels;
  }

  for (; x < width; ++x) {
    auto A16 = (float) half_to_float(data[permute0Value]);
    auto R16 = (float) half_to_float(data[permute1Value]);
    auto G16 = (float) half_to_float(data[permute2Value]);
    auto B16 = (float) half_to_float(data[permute3Value]);
    auto R10 = (uint32_t) (clamp(round(R16 * range10), 0.0f, (float) range10));
    auto G10 = (uint32_t) (clamp(round(G16 * range10), 0.0f, (float) range10));
    auto B10 = (uint32_t) (clamp(round(B16 * range10), 0.0f, (float) range10));
    auto A10 = (uint32_t) clamp(round(A16 * 3.f), 0.0f, 3.0f);

    dst32[0] = (A10 << 30) | (R10 << 20) | (G10 << 10) | B10;

    data += 4;
    dst32 += 1;
  }
}

void
F32ToRGBA1010102HWYRow(const float *JXL_RESTRICT data, uint32_t *JXL_RESTRICT dst,
                       const uint32_t width,
                       const int *permuteMap) {
  float range10 = powf(2, 10) - 1;
  const FixedTag<float, 4> df;
  const Rebind<int32_t, FixedTag<float, 4>> di32;
  const FixedTag<uint32_t, 4> du;
  using V = Vec<decltype(df)>;
  using VU = Vec<decltype(du)>;
  const auto vRange10 = Set(df, range10);
  const auto zeros = Zero(df);
  const auto alphaMax = Set(df, 3.0);

  const int permute0Value = permuteMap[0];
  const int permute1Value = permuteMap[1];
  const int permute2Value = permuteMap[2];
  const int permute3Value = permuteMap[3];

  uint32_t x = 0;
  auto dst32 = reinterpret_cast<uint32_t *>(dst);
  int pixels = 4;
  for (; x + pixels < width; x += pixels) {
    V pixels1;
    V pixels2;
    V pixels3;
    V pixels4;
    LoadInterleaved4(df, reinterpret_cast<const float *>(data), pixels1, pixels2,
                     pixels3, pixels4);
    pixels1 = ClampRound(df, Mul(pixels1, vRange10), zeros,
                         vRange10);
    pixels2 = ClampRound(df, Mul(pixels2, vRange10), zeros,
                         vRange10);
    pixels3 = ClampRound(df, Mul(pixels3, vRange10), zeros,
                         vRange10);
    pixels4 = ClampRound(df, Mul(pixels4, alphaMax), zeros,
                         alphaMax);
    VU pixelsu1 = BitCast(du, ConvertTo(di32, pixels1));
    VU pixelsu2 = BitCast(du, ConvertTo(di32, pixels2));
    VU pixelsu3 = BitCast(du, ConvertTo(di32, pixels3));
    VU pixelsu4 = BitCast(du, ConvertTo(di32, pixels4));

    VU pixelsStore[4] = {pixelsu1, pixelsu2, pixelsu3, pixelsu4};
    VU AV = pixelsStore[permute0Value];
    VU RV = pixelsStore[permute1Value];
    VU GV = pixelsStore[permute2Value];
    VU BV = pixelsStore[permute3Value];
    VU upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
    VU lower = Or(ShiftLeft<10>(GV), BV);
    VU final = Or(upper, lower);
    StoreU(final, du, dst32);
    data += pixels * 4;
    dst32 += pixels;
  }

  for (; x < width; ++x) {
    auto A16 = (float) data[permute0Value];
    auto R16 = (float) data[permute1Value];
    auto G16 = (float) data[permute2Value];
    auto B16 = (float) data[permute3Value];
    auto R10 = (uint32_t) (clamp(R16 * range10, 0.0f, (float) range10));
    auto G10 = (uint32_t) (clamp(G16 * range10, 0.0f, (float) range10));
    auto B10 = (uint32_t) (clamp(B16 * range10, 0.0f, (float) range10));
    auto A10 = (uint32_t) clamp(roundf(A16 * 3.f), 0.0f, 3.0f);

    dst32[0] = (A10 << 30) | (R10 << 20) | (G10 << 10) | B10;

    data += 4;
    dst32 += 1;
  }
}

template<typename D, typename R, typename T = Vec<D>, typename I = Vec<R>>
inline __attribute__((flatten)) Vec<D>
ConvertPixelsTo(D d, R rd, I r, I g, I b, I a, const int *permuteMap) {
  const T MulBy4Const = Set(d, 4);
  const T MulBy3Const = Set(d, 3);
  const T O127Const = Set(d, 127);
  T pixelsu1 = Mul(BitCast(d, r), MulBy4Const);
  T pixelsu2 = Mul(BitCast(d, g), MulBy4Const);
  T pixelsu3 = Mul(BitCast(d, b), MulBy4Const);
  T pixelsu4 = ShiftRight<8>(Add(Mul(BitCast(d, a), MulBy3Const), O127Const));

  const int permute0Value = permuteMap[0];
  const int permute1Value = permuteMap[1];
  const int permute2Value = permuteMap[2];
  const int permute3Value = permuteMap[3];

  T pixelsStore[4] = {pixelsu1, pixelsu2, pixelsu3, pixelsu4};
  T AV = pixelsStore[permute0Value];
  T RV = pixelsStore[permute1Value];
  T GV = pixelsStore[permute2Value];
  T BV = pixelsStore[permute3Value];
  T upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
  T lower = Or(ShiftLeft<10>(GV), BV);
  T final = Or(upper, lower);
  return final;
}

void
Rgba8ToRGBA1010102HWYRow(const uint8_t *JXL_RESTRICT data, uint32_t *JXL_RESTRICT dst,
                         const uint32_t width,
                         const int *permuteMap,
                         const bool attenuateAlpha) {
  const FixedTag<uint8_t, 16> du8x16;
  const FixedTag<uint16_t, 8> du16x4;
  const Rebind<int32_t, FixedTag<uint8_t, 4>> di32;
  const FixedTag<uint32_t, 4> du;
  using VU8x16 = Vec<decltype(du8x16)>;
  using VU = Vec<decltype(du)>;

  const int permute0Value = permuteMap[0];
  const int permute1Value = permuteMap[1];
  const int permute2Value = permuteMap[2];
  const int permute3Value = permuteMap[3];

  uint32_t x = 0;
  auto dst32 = reinterpret_cast<uint32_t *>(dst);
  int pixels = 16;
  for (; x + pixels < width; x += pixels) {
    VU8x16 upixels1;
    VU8x16 upixels2;
    VU8x16 upixels3;
    VU8x16 upixels4;
    LoadInterleaved4(du8x16, reinterpret_cast<const uint8_t *>(data),
                     upixels1,
                     upixels2,
                     upixels3,
                     upixels4);

    if (attenuateAlpha) {
      upixels1 = AttenuateVec(du8x16, upixels1, upixels4);
      upixels2 = AttenuateVec(du8x16, upixels2, upixels4);
      upixels3 = AttenuateVec(du8x16, upixels3, upixels4);
    }

    VU final = ConvertPixelsTo(du, di32,
                               PromoteUpperTo(di32, PromoteUpperTo(du16x4, upixels1)),
                               PromoteUpperTo(di32, PromoteUpperTo(du16x4, upixels2)),
                               PromoteUpperTo(di32, PromoteUpperTo(du16x4, upixels3)),
                               PromoteUpperTo(di32, PromoteUpperTo(du16x4, upixels4)),
                               permuteMap);
    StoreU(final, du, dst32 + 12);

    final = ConvertPixelsTo(du, di32,
                            PromoteLowerTo(di32, PromoteUpperTo(du16x4, upixels1)),
                            PromoteLowerTo(di32, PromoteUpperTo(du16x4, upixels2)),
                            PromoteLowerTo(di32, PromoteUpperTo(du16x4, upixels3)),
                            PromoteLowerTo(di32, PromoteUpperTo(du16x4, upixels4)),
                            permuteMap);
    StoreU(final, du, dst32 + 8);

    final = ConvertPixelsTo(du, di32,
                            PromoteUpperTo(di32, PromoteLowerTo(du16x4, upixels1)),
                            PromoteUpperTo(di32, PromoteLowerTo(du16x4, upixels2)),
                            PromoteUpperTo(di32, PromoteLowerTo(du16x4, upixels3)),
                            PromoteUpperTo(di32, PromoteLowerTo(du16x4, upixels4)),
                            permuteMap);
    StoreU(final, du, dst32 + 4);

    final = ConvertPixelsTo(du, di32,
                            PromoteLowerTo(di32, PromoteLowerTo(du16x4, upixels1)),
                            PromoteLowerTo(di32, PromoteLowerTo(du16x4, upixels2)),
                            PromoteLowerTo(di32, PromoteLowerTo(du16x4, upixels3)),
                            PromoteLowerTo(di32, PromoteLowerTo(du16x4, upixels4)),
                            permuteMap);
    StoreU(final, du, dst32);

    data += pixels * 4;
    dst32 += pixels;
  }

  for (; x < width; ++x) {
    uint8_t alpha = data[permute0Value];
    uint8_t r = data[permute1Value];
    uint8_t g = data[permute2Value];
    uint8_t b = data[permute3Value];

    if (attenuateAlpha) {
      r = (r * alpha + 127) / 255;
      g = (g * alpha + 127) / 255;
      b = (b * alpha + 127) / 255;
    }

    auto A16 = ((uint32_t) alpha * 3 + 127) >> 8;
    auto R16 = (uint32_t) r * 4;
    auto G16 = (uint32_t) g * 4;
    auto B16 = (uint32_t) b * 4;

    dst32[0] = (A16 << 30) | (R16 << 20) | (G16 << 10) | B16;
    data += 4;
    dst32 += 1;
  }
}

void
Rgba8ToRGBA1010102HWY(const uint8_t *JXL_RESTRICT source,
                      const int srcStride,
                      uint8_t *JXL_RESTRICT destination,
                      const int dstStride,
                      int width,
                      int height,
                      const bool attenuateAlpha) {
  int permuteMap[4] = {3, 2, 1, 0};

  auto src = reinterpret_cast<const uint8_t *>(source);

  concurrency::parallel_for(2, height, [&](int y) {
    Rgba8ToRGBA1010102HWYRow(
        reinterpret_cast<const uint8_t *>(src + srcStride * y),
        reinterpret_cast<uint32_t *>(destination + dstStride * y),
        width, &permuteMap[0], attenuateAlpha);
  });
}

void
F16ToRGBA1010102HWY(const uint16_t *JXL_RESTRICT source,
                    const uint32_t srcStride,
                    uint8_t *JXL_RESTRICT destination,
                    const uint32_t dstStride,
                    const uint32_t width,
                    const uint32_t height) {
  int permuteMap[4] = {3, 2, 1, 0};
  auto src = reinterpret_cast<const uint8_t *>(source);

  concurrency::parallel_for(2, height, [&](int y) {
    F16ToRGBA1010102HWYRow(
        reinterpret_cast<const uint16_t *>(src + srcStride * y),
        reinterpret_cast<uint32_t *>(destination + dstStride * y),
        width, &permuteMap[0]);
  });
}

void
F32ToRGBA1010102HWY(const float *JXL_RESTRICT source,
                    const uint32_t srcStride,
                    uint8_t *JXL_RESTRICT destination,
                    const uint32_t dstStride,
                    const uint32_t width,
                    const uint32_t height) {
  int permuteMap[4] = {3, 2, 1, 0};
  auto src = reinterpret_cast<const uint8_t *>(source);
  auto dst = reinterpret_cast<uint8_t *>(destination);

  concurrency::parallel_for(2, height, [&](int y) {
    F32ToRGBA1010102HWYRow(
        reinterpret_cast<const float *>(src + srcStride * y),
        reinterpret_cast<uint32_t *>(dst + dstStride * y),
        width, &permuteMap[0]);
  });
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace coder {
HWY_EXPORT(F16ToRGBA1010102HWY);
HWY_EXPORT(F32ToRGBA1010102HWY);
HWY_EXPORT(Rgba8ToRGBA1010102HWY);

HWY_DLLEXPORT void
F16ToRGBA1010102(const uint16_t *JXL_RESTRICT source, const uint32_t srcStride, uint8_t *JXL_RESTRICT destination, const uint32_t dstStride,
                 const uint32_t width, const uint32_t height) {
  HWY_DYNAMIC_DISPATCH(F16ToRGBA1010102HWY)(source, srcStride, destination, dstStride, width, height);
}

HWY_DLLEXPORT void
F32ToRGBA1010102(const float *JXL_RESTRICT source, const uint32_t srcStride, uint8_t *JXL_RESTRICT destination, const uint32_t dstStride,
                 const uint32_t width, const uint32_t height) {
  HWY_DYNAMIC_DISPATCH(F32ToRGBA1010102HWY)(source, srcStride, destination, dstStride, width, height);
}

HWY_DLLEXPORT void
Rgba8ToRGBA1010102(const uint8_t *JXL_RESTRICT source,
                   const uint32_t srcStride,
                   uint8_t *JXL_RESTRICT destination,
                   const uint32_t dstStride,
                   const uint32_t width,
                   const uint32_t height, const bool attenuateAlpha) {
  HWY_DYNAMIC_DISPATCH(Rgba8ToRGBA1010102HWY)(source, srcStride, destination, dstStride,
                                              width,
                                              height, attenuateAlpha);
}

template<typename V>
void RGBA1010102ToUnsigned(const uint8_t *JXL_RESTRICT src, const uint32_t srcStride,
                           V *JXL_RESTRICT dst, const uint32_t dstStride,
                           const uint32_t width, const uint32_t height, const uint32_t bitDepth) {
  auto mDstPointer = reinterpret_cast<uint8_t *>(dst);
  auto mSrcPointer = reinterpret_cast<const uint8_t *>(src);

  const float maxColors = std::powf(2.f, bitDepth) - 1.f;
  const float valueScale = maxColors / 1023.f;
  const float alphaValueScale = maxColors / 3.f;

  const uint32_t mask = (1u << 10u) - 1u;

  for (int y = 0; y < height; ++y) {

    auto dstPointer = reinterpret_cast<V *>(mDstPointer);
    auto srcPointer = reinterpret_cast<const uint8_t *>(mSrcPointer);

    for (int x = 0; x < width; ++x) {
      uint32_t rgba1010102 = reinterpret_cast<const uint32_t *>(srcPointer)[0];

      uint32_t r = (rgba1010102) & mask;
      uint32_t g = (rgba1010102 >> 10) & mask;
      uint32_t b = (rgba1010102 >> 20) & mask;
      uint32_t a1 = (rgba1010102 >> 30);

      V ru = std::clamp(static_cast<V>(std::roundf(r * valueScale)), static_cast<V>(0),
                        static_cast<V>(maxColors));
      V gu = std::clamp(static_cast<V>(std::roundf(g * valueScale)), static_cast<V>(0),
                        static_cast<V>(maxColors));
      V bu = std::clamp(static_cast<V>(std::roundf(b * valueScale)), static_cast<V>(0),
                        static_cast<V>(maxColors));
      V au = std::clamp(static_cast<V>(std::roundf(a1 * alphaValueScale)),
                        static_cast<V>(0), static_cast<V>(maxColors));

      // Alpha pre-multiplication for RGBA_8888
      if (std::is_same<V, uint8_t>::value) {
        ru = (ru * au + 127) / 255;
        gu = (gu * au + 127) / 255;
        bu = (bu * au + 127) / 255;
      }

      dstPointer[0] = ru;
      dstPointer[1] = gu;
      dstPointer[2] = bu;
      dstPointer[3] = au;

      srcPointer += 4;
      dstPointer += 4;
    }

    mSrcPointer += srcStride;
    mDstPointer += dstStride;
  }
}

template void RGBA1010102ToUnsigned(const uint8_t *JXL_RESTRICT src, const uint32_t srcStride,
                                    uint8_t *JXL_RESTRICT dst, const uint32_t dstStride,
                                    const uint32_t width, const uint32_t height, const uint32_t bitDepth);

template void RGBA1010102ToUnsigned(const uint8_t *JXL_RESTRICT src, const uint32_t srcStride,
                                    uint16_t *JXL_RESTRICT dst, const uint32_t dstStride,
                                    const uint32_t width, const uint32_t height, const uint32_t bitDepth);

}

#endif