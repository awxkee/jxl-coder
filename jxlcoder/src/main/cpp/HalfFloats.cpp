//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#include "HalfFloats.h"
#include <cstdint>
#include <__threading_support>
#include "ThreadPool.hpp"
#include <vector>

uint as_uint(const float x) {
    return *(uint *) &x;
}

float as_float(const uint x) {
    return *(float *) &x;
}

uint16_t float_to_half(
        const float x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint b =
            as_uint(x) + 0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
    const uint e = (b & 0x7F800000) >> 23; // exponent
    const uint m = b &
                   0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
    return (b & 0x80000000) >> 16 | (e > 112) * ((((e - 112) << 10) & 0x7C00) | m >> 13) |
           ((e < 113) & (e > 101)) * ((((0x007FF000 + m) >> (125 - e)) + 1) >> 1) |
           (e > 143) * 0x7FFF; // sign : normalized : denormalized : saturate
}

float half_to_float(
        const uint16_t x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint e = (x & 0x7C00) >> 10; // exponent
    const uint m = (x & 0x03FF) << 13; // mantissa
    const uint v = as_uint((float) m)
            >> 23; // evil log2 bit hack to count leading zeros in denormalized format
    return as_float((x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) | ((e == 0) & (m != 0)) *
                                                                            ((v - 37) << 23 |
                                                                             ((m << (150 - v)) &
                                                                              0x007FE000))); // sign : normalized : denormalized
}

#if HAVE_NEON

#include <arm_neon.h>

void RGBAfloat32ToF16ROWNEON(const float *src, uint16_t *dst, int width) {
    auto srcPixels = reinterpret_cast<const float *>(src);
    auto dstPixels = reinterpret_cast<uint16_t *>(dst);

    int x;
    for (x = 0; x + 2 < width; x += 2) {
        float32x4_t in0 = vld1q_f32(reinterpret_cast<const float *>(srcPixels));
        float32x4_t in1 = vld1q_f32(srcPixels + 4);

        float16x4_t out0 = vcvt_f16_f32(in0);
        float16x4_t out1 = vcvt_f16_f32(in1);

        vst1_u16(dstPixels, vreinterpret_u16_f16(out0));
        vst1_u16(dstPixels + 4, vreinterpret_u16_f16(out1));

        srcPixels += 8;
        dstPixels += 8;
    }

    for (; x < width; ++x) {
        dstPixels[0] = float_to_half(srcPixels[0]);
        dstPixels[1] = float_to_half(srcPixels[1]);
        dstPixels[2] = float_to_half(srcPixels[2]);
        dstPixels[3] = float_to_half(srcPixels[3]);

        dstPixels += 4;
        srcPixels += 4;
    }
}

void RgbaF32ToF16Neon(const float *src, int srcStride, uint16_t *dst, int dstStride,
                      int width, int height) {

    auto dstData = reinterpret_cast<uint8_t *>(dst);
    auto srcData = reinterpret_cast<const uint8_t *>(src);

    ThreadPool pool;
    std::vector<std::future<void>> results;

    for (int y = 0; y < height; ++y) {
        auto r = pool.enqueue(RGBAfloat32ToF16ROWNEON, reinterpret_cast<const float *>(srcData),
                              reinterpret_cast<uint16_t *>(dstData), width);
        results.push_back(std::move(r));
        dstData += dstStride;
        srcData += srcStride;
    }

    for (auto &result: results) {
        result.wait();
    }
}

#endif

void RGBAF32ToF16ROWC(const float *src, uint16_t *dst, int width) {
    for (int i = 0; i < width; i += 4) {
        dst[0] = float_to_half(src[0]);
        dst[1] = float_to_half(src[1]);
        dst[2] = float_to_half(src[2]);
        dst[3] = float_to_half(src[3]);

        src += 4;
        dst += 4;
    }
}

void
RgbaF32ToF16(const float *src, int srcStride, uint16_t *dst, int dstStride, int width,
             int height) {
    auto srcPixels = reinterpret_cast<const uint8_t *>(src);
    auto dstPixels = reinterpret_cast<uint8_t *>(dst);

    ThreadPool pool;
    std::vector<std::future<void>> results;

    for (int y = 0; y < height; ++y) {
        auto r = pool.enqueue(RGBAF32ToF16ROWC, reinterpret_cast<const float *>(srcPixels),
                              reinterpret_cast<uint16_t *>(dstPixels), width);
        results.push_back(std::move(r));
        srcPixels += srcStride;
        dstPixels += dstStride;
    }

    for (auto &result: results) {
        result.wait();
    }
}