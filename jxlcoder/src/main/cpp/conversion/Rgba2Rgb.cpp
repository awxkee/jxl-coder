/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 04/09/2023
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

#include "Rgba2Rgb.h"
#include <cstdint>
#include <vector>
#include <thread>
#include "concurrency.hpp"

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "conversion/Rgba2Rgb.cpp"

#include "hwy/foreach_target.h"  // IWYU pragma: keep

#include "hwy/highway.h"
#include "hwy/base.h"

HWY_BEFORE_NAMESPACE();
namespace coder::HWY_NAMESPACE {

using namespace hwy::HWY_NAMESPACE;

template<class D, typename V = VFromD<D>, typename T = TFromD<D>>
void
Rgba2RgbROW(D d, const T *JXL_RESTRICT src, T *JXL_RESTRICT dst, const uint32_t width) {
  uint32_t x = 0;
  auto srcPixels = reinterpret_cast<const T *>(src);
  auto dstPixels = reinterpret_cast<T *>(dst);
  const int pixels = d.MaxLanes();
  for (; x + pixels < width; x += pixels) {
    V pixels1;
    V pixels2;
    V pixels3;
    V pixels4;
    LoadInterleaved4(d, srcPixels, pixels1, pixels2, pixels3, pixels4);
    StoreInterleaved3(pixels1, pixels2, pixels3, d, dstPixels);

    srcPixels += 4 * pixels;
    dstPixels += 3 * pixels;
  }

  for (; x < width; ++x) {
    dstPixels[0] = srcPixels[0];
    dstPixels[1] = srcPixels[1];
    dstPixels[2] = srcPixels[2];

    srcPixels += 4;
    dstPixels += 3;
  }
}

template<class D, typename V = VFromD<D>, typename T = TFromD<D>>
void Rgba2RGBHWY(D d,
                 const T *JXL_RESTRICT src,
                 const uint32_t srcStride,
                 T *JXL_RESTRICT dst,
                 const uint32_t dstStride,
                 const uint32_t width,
                 const uint32_t height) {
  auto rgbaData = reinterpret_cast<const uint8_t *>(src);
  auto rgbData = reinterpret_cast<uint8_t *>(dst);

  concurrency::parallel_for(2, height, [&](int y) {
    Rgba2RgbROW(d, reinterpret_cast<const T *>(rgbaData + srcStride * y),
                reinterpret_cast<T *>(rgbData + dstStride * y), width);
  });
}

void Rgba2RGBHWYU8(const uint8_t *JXL_RESTRICT src,
                   const uint32_t srcStride,
                   uint8_t *JXL_RESTRICT dst,
                   const uint32_t dstStride,
                   const uint32_t width,
                   const uint32_t height) {
  const ScalableTag<uint8_t> t;
  Rgba2RGBHWY(t, src, srcStride, dst, dstStride, height, width);
}

void Rgba2RGBHWYU16(const uint16_t *JXL_RESTRICT src,
                    const uint32_t srcStride,
                    uint16_t *JXL_RESTRICT dst,
                    const uint32_t dstStride,
                    const uint32_t width,
                    const uint32_t height) {
  const ScalableTag<uint16_t> t;
  Rgba2RGBHWY(t, src, srcStride, dst, dstStride, height, width);
}

void Rgba2RGBHWYF32(const hwy::float32_t *JXL_RESTRICT src,
                    const uint32_t srcStride,
                    hwy::float32_t *JXL_RESTRICT dst,
                    const uint32_t dstStride,
                    const uint32_t width,
                    const uint32_t height) {
  const ScalableTag<hwy::float32_t> t;
  Rgba2RGBHWY(t, src, srcStride, dst, dstStride, height, width);
}

}
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace coder {
HWY_EXPORT(Rgba2RGBHWYU8);
HWY_EXPORT(Rgba2RGBHWYU16);
HWY_EXPORT(Rgba2RGBHWYF32);

template<class T>
HWY_DLLEXPORT void Rgba2RGB(const T *JXL_RESTRICT src,
                            const uint32_t srcStride,
                            T *JXL_RESTRICT dst,
                            const uint32_t dstStride,
                            const uint32_t width,
                            const uint32_t height) {
  if (std::is_same<T, uint8_t>::value || std::is_same<T, int8_t>::value || std::is_same<T, char>::value) {
    HWY_DYNAMIC_DISPATCH(Rgba2RGBHWYU8)(reinterpret_cast<const uint8_t *>(src),
                                        srcStride,
                                        reinterpret_cast<uint8_t *>(dst),
                                        dstStride,
                                        height,
                                        width);
  } else if (std::is_same<T, uint16_t>::value || std::is_same<T, hwy::float16_t>::value) {
    HWY_DYNAMIC_DISPATCH(Rgba2RGBHWYU16)(reinterpret_cast<const uint16_t *>(src),
                                         srcStride,
                                         reinterpret_cast<uint16_t *>(dst),
                                         dstStride,
                                         height,
                                         width);
  } else if (std::is_same<T, hwy::float32_t>::value || std::is_same<T, float>::value) {
    HWY_DYNAMIC_DISPATCH(Rgba2RGBHWYF32)(reinterpret_cast<const hwy::float32_t *>(src),
                                         srcStride,
                                         reinterpret_cast<hwy::float32_t *>(dst),
                                         dstStride,
                                         height,
                                         width);
  }
}

template void Rgba2RGB(const char *JXL_RESTRICT src,
                       const uint32_t srcStride,
                       char *JXL_RESTRICT dst,
                       const uint32_t dstStride,
                       const uint32_t width,
                       const uint32_t height);

template void Rgba2RGB(const int8_t *JXL_RESTRICT src,
                       const uint32_t srcStride,
                       int8_t *JXL_RESTRICT dst,
                       const uint32_t dstStride,
                       const uint32_t width,
                       const uint32_t height);

template void Rgba2RGB(const uint8_t *JXL_RESTRICT src,
                       const uint32_t srcStride,
                       uint8_t *JXL_RESTRICT dst,
                       const uint32_t dstStride,
                       const uint32_t width,
                       const uint32_t height);

template void Rgba2RGB(const uint16_t *JXL_RESTRICT src,
                       const uint32_t srcStride,
                       uint16_t *JXL_RESTRICT dst,
                       const uint32_t dstStride,
                       const uint32_t width,
                       const uint32_t height);

template void Rgba2RGB(const hwy::float16_t *JXL_RESTRICT src,
                       const uint32_t srcStride,
                       hwy::float16_t *JXL_RESTRICT dst,
                       const uint32_t dstStride, const uint32_t width, const uint32_t height);

template void Rgba2RGB(const float *JXL_RESTRICT src,
                       const uint32_t srcStride,
                       float *JXL_RESTRICT dst,
                       const uint32_t dstStride, const uint32_t width, const uint32_t height);

}

#endif