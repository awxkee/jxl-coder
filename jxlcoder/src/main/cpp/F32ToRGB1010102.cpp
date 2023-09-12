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

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "F32ToRGB1010102.cpp"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace coder {
    namespace HWY_NAMESPACE {

        using hwy::HWY_NAMESPACE::CappedTag;
        using hwy::HWY_NAMESPACE::Rebind;
        using hwy::HWY_NAMESPACE::Vec;
        using hwy::HWY_NAMESPACE::Set;
        using hwy::HWY_NAMESPACE::Zero;
        using hwy::HWY_NAMESPACE::Load;
        using hwy::HWY_NAMESPACE::Mul;
        using hwy::HWY_NAMESPACE::Max;
        using hwy::HWY_NAMESPACE::Min;
        using hwy::HWY_NAMESPACE::BitCast;
        using hwy::HWY_NAMESPACE::ExtractLane;
        using hwy::HWY_NAMESPACE::ConvertTo;

        void
        F32ToRGBA1010102RowC(const float *HWY_RESTRICT data, uint8_t *HWY_RESTRICT dst, int width,
                             const int *permuteMap) {
            float range10 = powf(2, 10) - 1;
            const CappedTag<float, 4> df;
            const Rebind<int32_t, CappedTag<float, 4>> di32;
            const CappedTag<uint32_t, 4> di;
            using V = Vec<decltype(df)>;
            const auto vRange10 = Set(df, range10);
            const auto zeros = Zero(df);
            int x;
            for (x = 0; x < width; ++x) {
                V color = Load(df, data);
                color = Mul(color, vRange10);
                color = Max(color, zeros);
                color = Min(color, vRange10);
                auto uint32Color = BitCast(di, ConvertTo(di32, color));
                auto A10 = (uint32_t) ExtractLane(uint32Color, permuteMap[0]);
                auto R10 = (uint32_t) ExtractLane(uint32Color, permuteMap[1]);
                auto G10 = (uint32_t) ExtractLane(uint32Color, permuteMap[2]);
                auto B10 = (uint32_t) ExtractLane(uint32Color, permuteMap[3]);
                auto destPixel = reinterpret_cast<uint32_t *>(dst);
                destPixel[0] = (A10 << 30) | (R10 << 20) | (G10 << 10) | B10;
                data += 4;
                dst += 4;
            }

            for (; x < width; ++x) {
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

            int minimumTilingAreaSize = 850 * 850;
            int currentAreaSize = width * height;
            if (minimumTilingAreaSize > currentAreaSize) {
                for (int y = 0; y < height; ++y) {
                    F32ToRGBA1010102RowC(
                            reinterpret_cast<const float *>(data.data() + srcStride * y),
                            newData.data() + newStride * y,
                            width, &permuteMap[0]);
                }
            } else {
                ThreadPool pool;
                std::vector<std::future<void>> results;

                for (int y = 0; y < height; ++y) {
                    auto r = pool.enqueue(F32ToRGBA1010102RowC,
                                          reinterpret_cast<const float *>(data.data() +
                                                                          srcStride * y),
                                          newData.data() + newStride * y,
                                          width, &permuteMap[0]);
                    results.push_back(std::move(r));
                }

                for (auto &result: results) {
                    result.wait();
                }

            }
            data = newData;
        }

// NOLINTNEXTLINE(google-readability-namespace-comments)
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace coder {
    HWY_EXPORT(F32ToRGBA1010102C);
    HWY_DLLEXPORT void
    F16ToRGBA1010102(std::vector<uint8_t> &data, int srcStride, int *dstStride, int width,
                     int height) {
        HWY_DYNAMIC_DISPATCH(F32ToRGBA1010102C)(data, srcStride, dstStride, width, height);
    }
}

#endif