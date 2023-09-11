//
// Created by Radzivon Bartoshyk on 11/09/2023.
//

#include "F32ToRGB1010102.h"
#include <vector>
#include "HalfFloats.h"
#include <arpa/inet.h>
#include <cmath>
#include <algorithm>
#include "ThreadPool.hpp"

#if HAVE_NEON

#include <arm_neon.h>

#ifdef __LITTLE_ENDIAN__

void F32ToRGBA1010102RowNEON(const float *data, uint8_t *dst, int width,
                             const int *permuteMap) {
    float range10 = powf(2, 10) - 1;
    float32_t rangeMultiplier[] = {range10, range10, range10,
                                   3}; // Replace with your desired scalar values

    float32x4_t range = vld1q_f32(rangeMultiplier);
    float32x4_t zeros = vdupq_n_f32(0);
    float32x4_t max = vdupq_n_f32(range10);
    int x;
    for (x = 0; x < width; ++x) {
        float32x4_t color = vld1q_f32(data);
        color = vmulq_f32(color, range);
        color = vmaxq_f32(color, zeros);
        color = vminq_f32(color, max);

        uint32x4_t uint32Color = vcvtq_u32_f32(color);
        auto A10 = (uint32_t) uint32Color[permuteMap[0]];
        auto R10 = (uint32_t) uint32Color[permuteMap[1]];
        auto G10 = (uint32_t) uint32Color[permuteMap[2]];
        auto B10 = (uint32_t) uint32Color[permuteMap[3]];
        auto destPixel = reinterpret_cast<uint32_t *>(dst);
        destPixel[0] = (A10 << 30) | (R10 << 20) | (G10 << 10) | B10;
        data += 4;
        dst += 4;
    }

}

#endif

#endif

void F32ToRGBA1010102RowC(const float *data, uint8_t *dst, int width,
                          const int *permuteMap) {
    float range10 = powf(2, 10) - 1;
    for (int x = 0; x < width; ++x) {
        auto A16 = (float) data[permuteMap[0]];
        auto R16 = (float) data[permuteMap[1]];
        auto G16 = (float) data[permuteMap[2]];
        auto B16 = (float) data[permuteMap[3]];
        auto R10 = (uint32_t) std::clamp(R16 * range10, 0.0f, (float) range10);
        auto G10 = (uint32_t) std::clamp(G16 * range10, 0.0f, (float) range10);
        auto B10 = (uint32_t) std::clamp(B16 * range10, 0.0f, (float) range10);
        auto A10 = (uint32_t) std::clamp(std::round(A16 * 3), 0.0f, 3.0f);

        auto destPixel = reinterpret_cast<uint32_t *>(dst);
        destPixel[0] = (A10 << 30) | (R10 << 20) | (G10 << 10) | B10;
        data += 4;
        dst += 4;
    }
}

void F32ToRGBA1010102C(std::vector<uint8_t> &data, int srcStride, int *dstStride, int width,
                       int height) {
    int newStride = (int) width * 4 * (int) sizeof(uint8_t);
    *dstStride = newStride;
    std::vector<uint8_t> newData(newStride * height);
    int permuteMap[4] = {3, 2, 1, 0};

    ThreadPool pool;
    std::vector<std::future<void>> results;

    for (int y = 0; y < height; ++y) {
#if HAVE_NEON
        auto r = pool.enqueue(F32ToRGBA1010102RowNEON,
                              reinterpret_cast<const float *>(data.data() + srcStride * y),
                              newData.data() + newStride * y,
                              width, &permuteMap[0]);
        results.push_back(std::move(r));
#else
        auto r = pool.enqueue(F32ToRGBA1010102RowC,
                              reinterpret_cast<const float *>(data.data() + srcStride * y),
                              newData.data() + newStride * y,
                              width, &permuteMap[0]);
        results.push_back(std::move(r));
#endif
    }

    for (auto &result: results) {
        result.wait();
    }

    data = newData;
}

void
F16ToRGBA1010102(std::vector<uint8_t> &data, int srcStride, int *dstStride, int width, int height) {
    F32ToRGBA1010102C(data, srcStride, dstStride, width, height);
}