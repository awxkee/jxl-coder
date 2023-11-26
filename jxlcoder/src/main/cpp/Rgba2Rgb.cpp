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

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "Rgba2Rgb.cpp"

#include <hwy/foreach_target.h>  // IWYU pragma: keep

#include <hwy/highway.h>
#include "hwy/base.h"

#include <android/log.h>

HWY_BEFORE_NAMESPACE();
namespace coder::HWY_NAMESPACE {

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
        for (; x + pixels < width; x += pixels) {
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

        int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                    width * height / (256 * 256)), 1, 12);
        vector<thread> workers;

        int segmentHeight = height / threadCount;

        for (int i = 0; i < threadCount; i++) {
            int start = i * segmentHeight;
            int end = (i + 1) * segmentHeight;
            if (i == threadCount - 1) {
                end = height;
            }
            workers.emplace_back([start, end, rgbaData, srcStride, dstStride, width, rgbData]() {
                for (int y = start; y < end; ++y) {
                    Rgba16bitToRGBC(reinterpret_cast<const uint16_t *>(rgbaData + srcStride * y),
                                    reinterpret_cast<uint16_t *>(rgbData + dstStride * y), width);
                }
            });
        }

        for (std::thread &thread: workers) {
            thread.join();
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