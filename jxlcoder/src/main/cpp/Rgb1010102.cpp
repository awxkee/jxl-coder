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
#include "HalfFloats.h"
#include <thread>

using namespace std;

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "Rgb1010102.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

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
    using hwy::float16_t;

    void
    F16ToRGBA1010102HWYRow(const uint16_t *HWY_RESTRICT data, uint32_t *HWY_RESTRICT dst,
                           int width,
                           const int *permuteMap) {
        float range10 = powf(2, 10) - 1;
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

        int x = 0;
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
            V pixelsLow1 = Min(
                    Max(Round(Mul(PromoteLowerTo(rbf32, fpixels1), vRange10)), zeros),
                    vRange10);
            V pixelsLow2 = Min(
                    Max(Round(Mul(PromoteLowerTo(rbf32, fpixels2), vRange10)), zeros),
                    vRange10);
            V pixelsLow3 = Min(
                    Max(Round(Mul(PromoteLowerTo(rbf32, fpixels3), vRange10)), zeros),
                    vRange10);
            V pixelsLow4 = Min(
                    Max(Round(Mul(PromoteLowerTo(rbf32, fpixels4), alphaMax)), zeros),
                    alphaMax);
            VU pixelsuLow1 = BitCast(du, ConvertTo(di32, pixelsLow1));
            VU pixelsuLow2 = BitCast(du, ConvertTo(di32, pixelsLow2));
            VU pixelsuLow3 = BitCast(du, ConvertTo(di32, pixelsLow3));
            VU pixelsuLow4 = BitCast(du, ConvertTo(di32, pixelsLow4));

            V pixelsHigh1 = Min(
                    Max(Round(Mul(PromoteUpperTo(rbf32, fpixels1), vRange10)), zeros),
                    vRange10);
            V pixelsHigh2 = Min(
                    Max(Round(Mul(PromoteUpperTo(rbf32, fpixels2), vRange10)), zeros),
                    vRange10);
            V pixelsHigh3 = Min(
                    Max(Round(Mul(PromoteUpperTo(rbf32, fpixels3), vRange10)), zeros),
                    vRange10);
            V pixelsHigh4 = Min(
                    Max(Round(Mul(PromoteUpperTo(rbf32, fpixels4), alphaMax)), zeros),
                    alphaMax);
            VU pixelsuHigh1 = BitCast(du, ConvertTo(di32, pixelsHigh1));
            VU pixelsuHigh2 = BitCast(du, ConvertTo(di32, pixelsHigh2));
            VU pixelsuHigh3 = BitCast(du, ConvertTo(di32, pixelsHigh3));
            VU pixelsuHigh4 = BitCast(du, ConvertTo(di32, pixelsHigh4));

            VU pixelsHighStore[4] = {pixelsuHigh1, pixelsuHigh2, pixelsuHigh3, pixelsuHigh4};
            VU AV = pixelsHighStore[permuteMap[0]];
            VU RV = pixelsHighStore[permuteMap[1]];
            VU GV = pixelsHighStore[permuteMap[2]];
            VU BV = pixelsHighStore[permuteMap[3]];
            VU upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
            VU lower = Or(ShiftLeft<10>(GV), BV);
            VU final = Or(upper, lower);
            StoreU(final, du, dst32 + 4);

            VU pixelsLowStore[4] = {pixelsuLow1, pixelsuLow2, pixelsuLow3, pixelsuLow4};
            AV = pixelsLowStore[permuteMap[0]];
            RV = pixelsLowStore[permuteMap[1]];
            GV = pixelsLowStore[permuteMap[2]];
            BV = pixelsLowStore[permuteMap[3]];
            upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
            lower = Or(ShiftLeft<10>(GV), BV);
            final = Or(upper, lower);
            StoreU(final, du, dst32);

            data += pixels * 4;
            dst32 += pixels;
        }

        for (; x < width; ++x) {
            auto A16 = (float) half_to_float(data[permuteMap[0]]);
            auto R16 = (float) half_to_float(data[permuteMap[1]]);
            auto G16 = (float) half_to_float(data[permuteMap[2]]);
            auto B16 = (float) half_to_float(data[permuteMap[3]]);
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
    F32ToRGBA1010102HWYRow(const float *HWY_RESTRICT data, uint32_t *HWY_RESTRICT dst,
                           int width,
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

        int x = 0;
        auto dst32 = reinterpret_cast<uint32_t *>(dst);
        int pixels = 4;
        for (; x + pixels < width; x += pixels) {
            V pixels1;
            V pixels2;
            V pixels3;
            V pixels4;
            LoadInterleaved4(df, reinterpret_cast<const float *>(data), pixels1, pixels2,
                             pixels3, pixels4);
            pixels1 = Min(
                    Max(Round(Mul(pixels1, vRange10)), zeros),
                    vRange10);
            pixels2 = Min(
                    Max(Round(Mul(pixels2, vRange10)), zeros),
                    vRange10);
            pixels3 = Min(
                    Max(Round(Mul(pixels3, vRange10)), zeros),
                    vRange10);
            pixels4 = Min(
                    Max(Round(Mul(pixels4, alphaMax)), zeros),
                    alphaMax);
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
            data += pixels * 4;
            dst32 += pixels;
        }

        for (; x < width; ++x) {
            auto A16 = (float) data[permuteMap[0]];
            auto R16 = (float) data[permuteMap[1]];
            auto G16 = (float) data[permuteMap[2]];
            auto B16 = (float) data[permuteMap[3]];
            auto R10 = (uint32_t) (clamp(R16 * range10, 0.0f, (float) range10));
            auto G10 = (uint32_t) (clamp(G16 * range10, 0.0f, (float) range10));
            auto B10 = (uint32_t) (clamp(B16 * range10, 0.0f, (float) range10));
            auto A10 = (uint32_t) clamp(roundf(A16 * 3.f), 0.0f, 3.0f);

            dst32[0] = (A10 << 30) | (R10 << 20) | (G10 << 10) | B10;

            data += 4;
            dst32 += 1;
        }
    }

    void
    Rgba8ToRGBA1010102HWYRow(const uint8_t *HWY_RESTRICT data, uint32_t *HWY_RESTRICT dst,
                             int width,
                             const int *permuteMap) {
        const FixedTag<uint8_t, 4> du8x4;
        const FixedTag<uint8_t, 8> du8x8;
        const Rebind<int32_t, FixedTag<uint8_t, 4>> di32;
        const FixedTag<uint32_t, 4> du;
        using VU8 = Vec<decltype(du8x8)>;
        using VU = Vec<decltype(du)>;

        int x = 0;
        auto dst32 = reinterpret_cast<uint32_t *>(dst);
        int pixels = 8;
        for (; x + pixels < width; x += pixels) {
            VU8 upixels1;
            VU8 upixels2;
            VU8 upixels3;
            VU8 upixels4;
            LoadInterleaved4(du8x8, reinterpret_cast<const uint8_t *>(data), upixels1, upixels2,
                             upixels3, upixels4);

            VU pixelsu1 = Mul(BitCast(du, PromoteTo(di32, UpperHalf(du8x4, upixels1))),
                              Set(du, 4));
            VU pixelsu2 = Mul(BitCast(du, PromoteTo(di32, UpperHalf(du8x4, upixels2))),
                              Set(du, 4));
            VU pixelsu3 = Mul(BitCast(du, PromoteTo(di32, UpperHalf(du8x4, upixels3))),
                              Set(du, 4));
            VU pixelsu4 = ShiftRight<8>(
                    Add(Mul(BitCast(du, PromoteTo(di32, UpperHalf(du8x4, upixels4))),
                            Set(du, 3)), Set(du, 127)));

            VU pixelsStore[4] = {pixelsu1, pixelsu2, pixelsu3, pixelsu4};
            VU AV = pixelsStore[permuteMap[0]];
            VU RV = pixelsStore[permuteMap[1]];
            VU GV = pixelsStore[permuteMap[2]];
            VU BV = pixelsStore[permuteMap[3]];
            VU upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
            VU lower = Or(ShiftLeft<10>(GV), BV);
            VU final = Or(upper, lower);
            StoreU(final, du, dst32 + 4);

            pixelsu1 = Mul(BitCast(du, PromoteTo(di32, LowerHalf(upixels1))),
                           Set(du, 4));
            pixelsu2 = Mul(BitCast(du, PromoteTo(di32, LowerHalf(upixels2))),
                           Set(du, 4));
            pixelsu3 = Mul(BitCast(du, PromoteTo(di32, LowerHalf(upixels3))),
                           Set(du, 4));
            pixelsu4 = ShiftRight<8>(
                    Add(Mul(BitCast(du, PromoteTo(di32, LowerHalf(upixels4))),
                            Set(du, 3)), Set(du, 127)));

            VU pixelsLowStore[4] = {pixelsu1, pixelsu2, pixelsu3, pixelsu4};
            AV = pixelsLowStore[permuteMap[0]];
            RV = pixelsLowStore[permuteMap[1]];
            GV = pixelsLowStore[permuteMap[2]];
            BV = pixelsLowStore[permuteMap[3]];
            upper = Or(ShiftLeft<30>(AV), ShiftLeft<20>(RV));
            lower = Or(ShiftLeft<10>(GV), BV);
            final = Or(upper, lower);
            StoreU(final, du, dst32);

            data += pixels * 4;
            dst32 += pixels;
        }

        for (; x < width; ++x) {
            auto A16 = ((uint32_t) data[permuteMap[0]] * 3 + 127) >> 8;
            auto R16 = (uint32_t) data[permuteMap[1]] * 4;
            auto G16 = (uint32_t) data[permuteMap[2]] * 4;
            auto B16 = (uint32_t) data[permuteMap[3]] * 4;

            dst32[0] = (A16 << 30) | (R16 << 20) | (G16 << 10) | B16;
            data += 4;
            dst32 += 1;
        }
    }

    void
    Rgba8ToRGBA1010102HWY(const uint8_t *source,
                          int srcStride,
                          uint8_t *destination,
                          int dstStride,
                          int width,
                          int height) {
        int permuteMap[4] = {3, 2, 1, 0};

        auto src = reinterpret_cast<const uint8_t *>(source);

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
            workers.emplace_back(
                    [start, end, src, destination, srcStride, dstStride, width, permuteMap]() {
                        for (int y = start; y < end; ++y) {
                            Rgba8ToRGBA1010102HWYRow(
                                    reinterpret_cast<const uint8_t *>(src + srcStride * y),
                                    reinterpret_cast<uint32_t *>(destination + dstStride * y),
                                    width, &permuteMap[0]);
                        }
                    });
        }

        for (std::thread &thread: workers) {
            thread.join();
        }
    }

    void
    F16ToRGBA1010102HWY(const uint16_t *source,
                        int srcStride,
                        uint8_t *destination,
                        int dstStride,
                        int width,
                        int height) {
        int permuteMap[4] = {3, 2, 1, 0};
        auto src = reinterpret_cast<const uint8_t *>(source);

        int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                    width * height / (256 * 256)), 1, 12);
        std::vector<std::thread> workers;

        int segmentHeight = height / threadCount;

        for (int i = 0; i < threadCount; i++) {
            int start = i * segmentHeight;
            int end = (i + 1) * segmentHeight;
            if (i == threadCount - 1) {
                end = height;
            }
            workers.emplace_back(
                    [start, end, src, destination, srcStride, dstStride, width, permuteMap]() {
                        for (int y = start; y < end; ++y) {
                            F16ToRGBA1010102HWYRow(
                                    reinterpret_cast<const uint16_t *>(src + srcStride * y),
                                    reinterpret_cast<uint32_t *>(destination + dstStride * y),
                                    width, &permuteMap[0]);
                        }
                    });
        }

        for (std::thread &thread: workers) {
            thread.join();
        }
    }

    void
    F32ToRGBA1010102HWY(const float *source,
                        int srcStride,
                        uint8_t *destination,
                        int dstStride,
                        int width,
                        int height) {
        int permuteMap[4] = {3, 2, 1, 0};
        auto src = reinterpret_cast<const uint8_t *>(source);
        auto dst = reinterpret_cast<uint8_t *>(destination);

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
                    [start, end, src, dst, srcStride, dstStride, width, permuteMap]() {
                        for (int y = start; y < end; ++y) {
                            F32ToRGBA1010102HWYRow(
                                    reinterpret_cast<const float *>(src + srcStride * y),
                                    reinterpret_cast<uint32_t *>(dst + dstStride * y),
                                    width, &permuteMap[0]);
                        }
                    });
        }

        for (std::thread &thread: workers) {
            thread.join();
        }
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
    F16ToRGBA1010102(const uint16_t *source, int srcStride, uint8_t *destination, int dstStride,
                     int width,
                     int height) {
        HWY_DYNAMIC_DISPATCH(F16ToRGBA1010102HWY)(source, srcStride, destination, dstStride, width,
                                                  height);
    }

    HWY_DLLEXPORT void
    F32ToRGBA1010102(const float *source, int srcStride, uint8_t *destination, int dstStride,
                     int width,
                     int height) {
        HWY_DYNAMIC_DISPATCH(F32ToRGBA1010102HWY)(source, srcStride, destination, dstStride, width,
                                                  height);
    }

    HWY_DLLEXPORT void
    Rgba8ToRGBA1010102(const uint8_t *source,
                       int srcStride,
                       uint8_t *destination,
                       int dstStride,
                       int width,
                       int height) {
        HWY_DYNAMIC_DISPATCH(Rgba8ToRGBA1010102HWY)(source, srcStride, destination, dstStride,
                                                    width,
                                                    height);
    }

}

#endif