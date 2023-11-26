/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 13/09/2023
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

#include "Rgb1010102toF16.h"
#include <cstdint>
#include <HalfFloats.h>
#include <vector>
#include <algorithm>
#include <thread>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "Rgba1010102toF32.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

using namespace std;

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

        using hwy::HWY_NAMESPACE::FixedTag;
        using hwy::HWY_NAMESPACE::Vec;
        using hwy::HWY_NAMESPACE::LoadU;
        using hwy::float16_t;
        using hwy::HWY_NAMESPACE::RebindToFloat;
        using hwy::HWY_NAMESPACE::Rebind;
        using hwy::HWY_NAMESPACE::ShiftRight;
        using hwy::HWY_NAMESPACE::ShiftRightSame;
        using hwy::HWY_NAMESPACE::Set;
        using hwy::HWY_NAMESPACE::And;
        using hwy::HWY_NAMESPACE::ConvertTo;
        using hwy::HWY_NAMESPACE::DemoteTo;
        using hwy::HWY_NAMESPACE::StoreInterleaved4;
        using hwy::HWY_NAMESPACE::Div;
        using hwy::HWY_NAMESPACE::BitCast;

        void
        ConvertRGBA1010102toF32HWYRow(const uint8_t *HWY_RESTRICT src, float *HWY_RESTRICT dst,
                                      int width, bool littleEndian) {

            const FixedTag<uint32_t, 4> du32;
            const FixedTag<float, 4> df32;

            using VU32 = Vec<decltype(du32)>;

            auto dstPointer = reinterpret_cast<float *>(dst);
            auto srcPointer = reinterpret_cast<const uint32_t *>(src);

            auto mask = Set(du32, (1u << 10u) - 1u);

            int x = 0;

            const RebindToFloat<decltype(du32)> dfcf16;

            auto maxColors = (float) (powf(2.0f, 10.0f) - 1.0f);
            auto maxColorsF32 = Set(df32, maxColors);
            auto maxColorsAF32 = Set(df32, 3.0f);

            for (x = 0; x + 4 < width; x += 4) {
                VU32 color1010102 = LoadU(du32, reinterpret_cast<const uint32_t *>(srcPointer));
                auto RF = ConvertTo(dfcf16, And(ShiftRight<20>(color1010102), mask));
                auto RF16 = Div(RF, maxColorsF32);
                auto GF = ConvertTo(dfcf16, And(ShiftRight<10>(color1010102), mask));
                auto GF16 = Div(GF, maxColorsF32);
                auto BF = ConvertTo(dfcf16, And(color1010102, mask));
                auto BF16 = Div(BF, maxColorsF32);
                auto AU = ConvertTo(dfcf16, And(ShiftRight<30>(color1010102), mask));
                auto AF16 = Div(AU, maxColorsAF32);
                StoreInterleaved4(RF16,
                                  GF16,
                                  BF16,
                                  AF16,
                                  df32,
                                  dstPointer);

                srcPointer += 4 * 4;
                dstPointer += 4 * 4;
            }

            for (; x < width; ++x) {
                uint32_t rgba1010102 = reinterpret_cast<const uint32_t *>(srcPointer)[0];

                const uint32_t scalarMask = (1u << 10u) - 1u;
                uint32_t b = (rgba1010102) & scalarMask;
                uint32_t g = (rgba1010102 >> 10) & scalarMask;
                uint32_t r = (rgba1010102 >> 20) & scalarMask;
                uint32_t a = (rgba1010102 >> 30) * 3;  // Replicate 2 bits to 8 bits.

                // Convert each channel to floating-point values
                float rFloat = static_cast<float>(r) / 1023.0f;
                float gFloat = static_cast<float>(g) / 1023.0f;
                float bFloat = static_cast<float>(b) / 1023.0f;
                float aFloat = static_cast<float>(a) / 3.0f;

                auto dstCast = reinterpret_cast<float *>(dstPointer);
                if (littleEndian) {
                    dstCast[0] = bFloat;
                    dstCast[1] = gFloat;
                    dstCast[2] = rFloat;
                    dstCast[3] = aFloat;
                } else {
                    dstCast[0] = rFloat;
                    dstCast[1] = gFloat;
                    dstCast[2] = bFloat;
                    dstCast[3] = aFloat;
                }

                srcPointer += 1;
                dstPointer += 4;
            }
        }

        void
        ConvertRGBA1010102toF32(const uint8_t *HWY_RESTRICT src, int srcStride,
                                float *HWY_RESTRICT dst, int dstStride,
                                int width, int height) {
            auto mDstPointer = reinterpret_cast<uint8_t *>(dst);
            auto mSrcPointer = reinterpret_cast<const uint8_t *>(src);

            uint32_t testValue = 0x01020304;
            auto testBytes = reinterpret_cast<uint8_t *>(&testValue);

            bool littleEndian = false;
            if (testBytes[0] == 0x04) {
                littleEndian = true;
            } else if (testBytes[0] == 0x01) {
                littleEndian = false;
            }

            int threadCount = clamp(min(static_cast<int>(thread::hardware_concurrency()),
                                        width * height / (256 * 256)), 1, 12);
            vector<thread> workers;

            int segmentHeight = height / threadCount;

            for (int i = 0; i < threadCount; i++) {
                int start = i * segmentHeight;
                int end = (i + 1) * segmentHeight;
                if (i == threadCount - 1) {
                    end = height;
                }
                workers.emplace_back(
                        [start, end, mSrcPointer, mDstPointer, srcStride, dstStride, width, littleEndian]() {
                            for (int y = start; y < end; ++y) {
                                ConvertRGBA1010102toF32HWYRow(
                                        reinterpret_cast<const uint8_t *>(mSrcPointer + srcStride * y),
                                        reinterpret_cast<float *>(mDstPointer + dstStride * y),
                                        width, littleEndian);
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
    HWY_EXPORT(ConvertRGBA1010102toF32);

    HWY_DLLEXPORT void
    ConvertRGBA1010102toF32(const uint8_t *HWY_RESTRICT src, int srcStride,
                            float *HWY_RESTRICT dst, int dstStride,
                            int width, int height) {
        HWY_DYNAMIC_DISPATCH(ConvertRGBA1010102toF32)(src, srcStride, dst, dstStride, width,
                                                      height);
    }
}

#endif