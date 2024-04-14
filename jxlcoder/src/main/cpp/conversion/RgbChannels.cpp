//
// Created by Radzivon Bartoshyk on 10/03/2024.
//

#include "RgbChannels.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "conversion/RgbChannels.cpp"

#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/base.h"
#include "concurrency.hpp"

HWY_BEFORE_NAMESPACE();
namespace coder::HWY_NAMESPACE {

using namespace hwy::HWY_NAMESPACE;

template<class D, typename V = VFromD<D>, typename T = TFromD<D>>
void
RgbaPickChannelRowHWY(D d, const T *JXL_RESTRICT src, T *JXL_RESTRICT dst, const uint32_t width, const uint32_t channel) {
  uint32_t x = 0;
  auto srcPixels = reinterpret_cast<const T *>(src);
  auto dstPixels = reinterpret_cast<T *>(dst);
  const int pixels = Lanes(d);
  for (; x + pixels < width; x += pixels) {
    V pixels1;
    V pixels2;
    V pixels3;
    V pixels4;
    LoadInterleaved4(d, srcPixels, pixels1, pixels2, pixels3, pixels4);
    V channels[4] = {pixels1, pixels2, pixels3, pixels4};
    StoreU(channels[channel], d, dstPixels);

    srcPixels += 4 * pixels;
    dstPixels += 1 * pixels;
  }

  for (; x < width; ++x) {
    dstPixels[0] = srcPixels[channel];
    srcPixels += 4;
    dstPixels += 1;
  }
}

template<class D, typename V = VFromD<D>, typename T = TFromD<D>>
void RGBAPickChannelHWY(D d,
                        const T *JXL_RESTRICT src,
                        const uint32_t srcStride,
                        T *JXL_RESTRICT dst,
                        const uint32_t dstStride,
                        const uint32_t width,
                        const uint32_t height,
                        const uint32_t channel) {
  auto rgbaData = reinterpret_cast<const uint8_t *>(src);
  auto rgbData = reinterpret_cast<uint8_t *>(dst);

  for (int y = 0; y < height; ++y) {
    RgbaPickChannelRowHWY(d, reinterpret_cast<const T *>(rgbaData + srcStride * y),
                          reinterpret_cast<T *>(rgbData + dstStride * y), width, channel);
  }
}

void RGBAPickChannelUnsigned8HWY(const uint8_t *JXL_RESTRICT src,
                                 const uint32_t srcStride,
                                 uint8_t *JXL_RESTRICT dst,
                                 const uint32_t dstStride,
                                 const uint32_t width,
                                 const uint32_t height,
                                 const uint32_t channel) {
  const ScalableTag<uint8_t> t;
  RGBAPickChannelHWY(t, src, srcStride, dst, dstStride, height, width, channel);
}

void RGBAPickChannelUnsigned16HWY(const uint16_t *JXL_RESTRICT src,
                                  const uint32_t srcStride,
                                  uint16_t *JXL_RESTRICT dst,
                                  const uint32_t dstStride,
                                  const uint32_t width,
                                  const uint32_t height,
                                  const uint32_t channel) {
  const ScalableTag<uint16_t> t;
  RGBAPickChannelHWY(t, src, srcStride, dst, dstStride, height, width, channel);
}

void RGBAPickChannelFloat32HWY(const hwy::float32_t *JXL_RESTRICT src,
                               const uint32_t srcStride,
                               hwy::float32_t *JXL_RESTRICT dst,
                               const uint32_t dstStride,
                               const uint32_t width,
                               const uint32_t height,
                               const uint32_t channel) {
  const ScalableTag<hwy::float32_t> t;
  RGBAPickChannelHWY(t, src, srcStride, dst, dstStride, height, width, channel);
}

}
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
HWY_EXPORT(RGBAPickChannelUnsigned8HWY);
HWY_EXPORT(RGBAPickChannelUnsigned16HWY);
HWY_EXPORT(RGBAPickChannelFloat32HWY);
template<class T>
void RGBAPickChannel(const T *JXL_RESTRICT src,
                     const uint32_t srcStride,
                     T *JXL_RESTRICT dst,
                     const uint32_t dstStride,
                     const uint32_t width,
                     const uint32_t height,
                     const uint32_t channel) {
  if (std::is_same<T, uint8_t>::value) {
    HWY_DYNAMIC_DISPATCH(RGBAPickChannelUnsigned8HWY)(reinterpret_cast<const uint8_t *>(src), srcStride,
                                                      reinterpret_cast<uint8_t *>(dst), dstStride,
                                                      height, width, channel);
  } else if (std::is_same<T, uint16_t>::value || std::is_same<T, hwy::float16_t>::value) {
    HWY_DYNAMIC_DISPATCH(RGBAPickChannelUnsigned16HWY)(reinterpret_cast<const uint16_t *>(src), srcStride,
                                                       reinterpret_cast<uint16_t *>(dst), dstStride,
                                                       height, width, channel);
  } else if (std::is_same<T, hwy::float32_t>::value || std::is_same<T, float>::value) {
    HWY_DYNAMIC_DISPATCH(RGBAPickChannelFloat32HWY)(reinterpret_cast<const hwy::float32_t *>(src), srcStride,
                                                    reinterpret_cast<hwy::float32_t *>(dst), dstStride,
                                                    height, width, channel);
  }
}

template
void RGBAPickChannel(const uint8_t *JXL_RESTRICT src,
                     const uint32_t srcStride,
                     uint8_t *JXL_RESTRICT dst,
                     const uint32_t dstStride,
                     const uint32_t width,
                     const uint32_t height,
                     const uint32_t channel);

template
void RGBAPickChannel(const uint16_t *JXL_RESTRICT src,
                     const uint32_t srcStride,
                     uint16_t *JXL_RESTRICT dst,
                     const uint32_t dstStride,
                     const uint32_t width,
                     const uint32_t height,
                     const uint32_t channel);

template
void RGBAPickChannel(const hwy::float16_t *JXL_RESTRICT src,
                     const uint32_t srcStride,
                     hwy::float16_t *JXL_RESTRICT dst,
                     const uint32_t dstStride,
                     const uint32_t width,
                     const uint32_t height,
                     const uint32_t channel);

template
void RGBAPickChannel(const hwy::float32_t *JXL_RESTRICT src,
                     const uint32_t srcStride,
                     hwy::float32_t *JXL_RESTRICT dst,
                     const uint32_t dstStride,
                     const uint32_t width,
                     const uint32_t height,
                     const uint32_t channel);

}

#endif