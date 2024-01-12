/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 11/09/2023
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

#include "F32ToRGB1010102.h"
#include <vector>
#include "HalfFloats.h"
#include <arpa/inet.h>
#include <cmath>
#include <algorithm>
#include <thread>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "F32ToRGB1010102.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

using namespace std;

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

    using hwy::HWY_NAMESPACE::FixedTag;
    using hwy::HWY_NAMESPACE::Rebind;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::HWY_NAMESPACE::Set;
    using hwy::HWY_NAMESPACE::Zero;
    using hwy::HWY_NAMESPACE::LoadU;
    using hwy::HWY_NAMESPACE::Mul;
    using hwy::HWY_NAMESPACE::Max;
    using hwy::HWY_NAMESPACE::Min;
    using hwy::HWY_NAMESPACE::BitCast;
    using hwy::HWY_NAMESPACE::ExtractLane;
    using hwy::HWY_NAMESPACE::DemoteTo;
    using hwy::HWY_NAMESPACE::ConvertTo;
    using hwy::HWY_NAMESPACE::LoadInterleaved4;
    using hwy::HWY_NAMESPACE::Or;
    using hwy::HWY_NAMESPACE::ShiftLeft;
    using hwy::HWY_NAMESPACE::StoreU;
    using hwy::HWY_NAMESPACE::Round;
    using hwy::float32_t;

    void
    F32ToRGBA1010102RowC(const float *HWY_RESTRICT data, uint8_t *HWY_RESTRICT dst, int width,
                         const int *permuteMap) {
        float range10 = powf(2, 10) - 1;
        const FixedTag<float32_t, 4> df;
        const Rebind<int32_t, FixedTag<float32_t, 4>> di32;
        const FixedTag<uint32_t, 4> du;
        using V = Vec<decltype(df)>;
        using VU = Vec<decltype(du)>;
        const auto vRange10 = Set(df, range10);
        const auto zeros = Zero(df);
        const auto alphaMax = Set(df, 3.0);
        int x = 0;
        auto dst32 = reinterpret_cast<uint32_t *>(dst);
        auto src = reinterpret_cast<const float32_t *>(data);
        int pixels = 4;
        for (; x + pixels < width; x += pixels) {
            V pixels1;
            V pixels2;
            V pixels3;
            V pixels4;
            LoadInterleaved4(df, src, pixels1, pixels2, pixels3, pixels4);
            pixels1 = Min(Max(Round(Mul(pixels1, vRange10)), zeros), vRange10);
            pixels2 = Min(Max(Round(Mul(pixels2, vRange10)), zeros), vRange10);
            pixels3 = Min(Max(Round(Mul(pixels3, vRange10)), zeros), vRange10);
            pixels4 = Min(Max(Round(Mul(pixels4, alphaMax)), zeros), alphaMax);

            VU pixelsu1 = BitCast(du, ConvertTo(di32, pixels1));
            VU pixelsu2 = BitCast(du, ConvertTo(di32, pixels2));
            VU pixelsu3 = BitCast(du, ConvertTo(di32, pixels3));
            VU pixelsu4 = BitCast(du, ConvertTo(di32, pixels4));

            VU pixelsStore[4] = {pixelsu1, pixelsu2, pixelsu3, pixelsu4};
            VU AV = pixelsStore[permuteMap[0]];
            VU RV = pixelsStore[permuteMap[1]];
            VU GV = pixelsStore[permuteMap[2]];
            VU BV = pixelsStore[permuteMap[3]];
            VU upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
            VU lower = Or(ShiftLeft<10>(GV), BV);
            VU final = Or(upper, lower);
            StoreU(final, du, dst32);
            src += 4 * pixels;
            dst32 += 4;
        }

        for (; x < width; ++x) {
            auto A16 = (float) src[permuteMap[0]];
            auto R16 = (float) src[permuteMap[1]];
            auto G16 = (float) src[permuteMap[2]];
            auto B16 = (float) src[permuteMap[3]];
            auto R10 = (uint32_t) (clamp(R16 * range10, 0.0f, (float) range10));
            auto G10 = (uint32_t) (clamp(G16 * range10, 0.0f, (float) range10));
            auto B10 = (uint32_t) (clamp(B16 * range10, 0.0f, (float) range10));
            auto A10 = (uint32_t) clamp(round(A16 * 3), 0.0f, 3.0f);

            dst32[0] = (A10 << 30) | (R10 << 20) | (G10 << 10) | B10;

            src += 4;
            dst32 += 1;
        }
    }

    void
    F32ToRGBA1010102HWY(vector <uint8_t> &data, int srcStride, int *HWY_RESTRICT dstStride,
                        int width,
                        int height) {
        int newStride = (int) width * 4 * (int) sizeof(uint8_t);
        *dstStride = newStride;
        vector<uint8_t> newData(newStride * height);
        int permuteMap[4] = {3, 2, 1, 0};

        int threadCount = clamp(min(static_cast<int>(thread::hardware_concurrency()),
                                    width * height / (256 * 256)), 1, 12);
        vector<thread> workers;

        int segmentHeight = height / threadCount;

        auto src = data.data();
        auto dst = newData.data();

        for (int i = 0; i < threadCount; i++) {
            int start = i * segmentHeight;
            int end = (i + 1) * segmentHeight;
            if (i == threadCount - 1) {
                end = height;
            }
            workers.emplace_back(
                    [start, end, src, dst, srcStride, dstStride, width, permuteMap]() {
                        for (int y = start; y < end; ++y) {
                            F32ToRGBA1010102RowC(
                                    reinterpret_cast<const float *>(src + srcStride * y),
                                    reinterpret_cast<uint8_t *>(dst + (*dstStride) * y),
                                    width, &permuteMap[0]);
                        }
                    });
        }

        for (std::thread &thread: workers) {
            thread.join();
        }

        data = newData;
    }

// NOLINTNEXTLINE(google-readability-namespace-comments)
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace coder {
    HWY_EXPORT(F32ToRGBA1010102HWY);
    HWY_DLLEXPORT void
    F32ToRGBA1010102(std::vector<uint8_t> &data, int srcStride, int *HWY_RESTRICT dstStride,
                     int width,
                     int height) {
        HWY_DYNAMIC_DISPATCH(F32ToRGBA1010102HWY)(data, srcStride, dstStride, width, height);
    }
}

#endif