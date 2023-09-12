//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#include "Rgba2Rgb.h"
#include <cstdint>
#include <vector>
#include "ThreadPool.hpp"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "Rgba2Rgb.cpp"
#include <hwy/foreach_target.h>  // IWYU pragma: keep

#include <hwy/highway.h>
#include "hwy/base.h"

#include <android/log.h>

HWY_BEFORE_NAMESPACE();
namespace coder {
    namespace HWY_NAMESPACE {

        using hwy::HWY_NAMESPACE::LoadInterleaved4;
        using hwy::HWY_NAMESPACE::StoreInterleaved3;
        using hwy::HWY_NAMESPACE::ScalableTag;
        using hwy::HWY_NAMESPACE::Vec;

        void
        Rgba16bitToRGBC(const uint16_t *HWY_RESTRICT src, uint16_t *HWY_RESTRICT dst, int width) {
            const ScalableTag<uint16_t> du16;
            using V = Vec<decltype(du16)>;
            int x = 0;
            auto srcPixels = reinterpret_cast<const uint16_t *>(src);
            auto dstPixels = reinterpret_cast<uint16_t *>(dst);
            int pixels = du16.MaxLanes();
            for (x = 0; x + pixels < width; x += pixels) {
                V pixels1;
                V pixels2;
                V pixels3;
                V pixels4;
                LoadInterleaved4(du16, srcPixels, pixels1, pixels2, pixels3, pixels4);
                StoreInterleaved3(pixels1, pixels2, pixels3, du16, dstPixels);

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

        void HRgba16bit2RGB(const uint16_t *HWY_RESTRICT src, int srcStride,
                            uint16_t *HWY_RESTRICT dst, int dstStride, int height,
                            int width) {
            auto rgbaData = reinterpret_cast<const uint8_t *>(src);
            auto rgbData = reinterpret_cast<uint8_t *>(dst);

            int minimumTilingAreaSize = 850 * 850;
            int currentAreaSize = width * height;

            if (minimumTilingAreaSize > currentAreaSize) {
                for (int y = 0; y < height; ++y) {
                    Rgba16bitToRGBC(reinterpret_cast<const uint16_t *>(rgbaData + srcStride * y),
                                    reinterpret_cast<uint16_t *>(rgbData + dstStride * y), width);
                }
            } else {
                ThreadPool pool;
                std::vector<std::future<void>> results;

                for (int y = 0; y < height; ++y) {
                    auto r = pool.enqueue(Rgba16bitToRGBC,
                                          reinterpret_cast<const uint16_t *>(rgbaData + srcStride * y),
                                          reinterpret_cast<uint16_t *>(rgbData + dstStride * y), width);
                    results.push_back(std::move(r));
                }

                for (auto &result: results) {
                    result.wait();
                }
            }
        }

    }
}
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace coder {
    HWY_EXPORT(Rgba16bitToRGBC);
    HWY_EXPORT(HRgba16bit2RGB);
    HWY_DLLEXPORT void Rgba16bit2RGB(const uint16_t *HWY_RESTRICT src, int srcStride,
                                     uint16_t *HWY_RESTRICT dst, int dstStride, int height,
                                     int width) {
        HWY_DYNAMIC_DISPATCH(HRgba16bit2RGB)(src, srcStride, dst, dstStride, height, width);
    }
}

#endif