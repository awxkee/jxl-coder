/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 29/09/2023
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

#include "XScaler.h"
#include "half.hpp"
#include <thread>
#include <vector>
#include "algo/sampler.h"

#if defined(__clang__)
#pragma clang fp contract(fast) exceptions(ignore) reassociate(on)
#endif

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "XScaler.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "algo/sampler-inl.h"

using namespace half_float;
using namespace std;

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

    using hwy::HWY_NAMESPACE::Set;
    using hwy::HWY_NAMESPACE::FixedTag;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::HWY_NAMESPACE::Add;
    using hwy::HWY_NAMESPACE::LoadU;
    using hwy::HWY_NAMESPACE::BitCast;
    using hwy::HWY_NAMESPACE::Min;
    using hwy::HWY_NAMESPACE::ConvertTo;
    using hwy::HWY_NAMESPACE::Floor;
    using hwy::HWY_NAMESPACE::Mul;
    using hwy::HWY_NAMESPACE::ExtractLane;
    using hwy::HWY_NAMESPACE::Zero;
    using hwy::HWY_NAMESPACE::StoreU;
    using hwy::HWY_NAMESPACE::Round;
    using hwy::HWY_NAMESPACE::LoadInterleaved4;
    using hwy::HWY_NAMESPACE::Sub;
    using hwy::HWY_NAMESPACE::Max;
    using hwy::HWY_NAMESPACE::Abs;
    using hwy::HWY_NAMESPACE::MulAdd;
    using hwy::HWY_NAMESPACE::SumOfLanes;
    using hwy::HWY_NAMESPACE::Div;
    using hwy::HWY_NAMESPACE::NegMulAdd;
    using hwy::HWY_NAMESPACE::IfThenElse;
    using hwy::HWY_NAMESPACE::MulSub;
    using hwy::HWY_NAMESPACE::Neg;
    using hwy::float32_t;
    using hwy::float16_t;

    typedef float (*KernelSample4Func)(float, float, float, float, float);

    typedef float (*KernelWindow2Func)(float, const float);

    void
    scaleRowF16(const uint8_t *src8,
                const int srcStride,
                const int dstStride, int inputWidth,
                int inputHeight,
                uint16_t *output,
                int outputWidth,
                const float xScale,
                const XSampler &option,
                const float yScale, int y, const int components) {
        auto dst8 = reinterpret_cast<uint8_t *>(output) + y * dstStride;
        auto dst16 = reinterpret_cast<uint16_t *>(dst8);

        const FixedTag<float32_t, 4> dfx4;
        const FixedTag<int32_t, 4> dix4;
        const FixedTag<float16_t, 4> df16x4;
        using VI4 = Vec<decltype(dix4)>;
        using VF4 = Vec<decltype(dfx4)>;
        using VF16x4 = Vec<decltype(df16x4)>;

        const int shift[4] = {0, 1, 2, 3};
        const VI4 shiftV = LoadU(dix4, shift);
        const VF4 xScaleV = Set(dfx4, xScale);
        const VF4 yScaleV = Set(dfx4, yScale);
        const VI4 addOne = Set(dix4, 1);
        const VF4 fOneV = Set(dfx4, 1.0f);
        const VI4 maxWidth = Set(dix4, inputWidth - 1);
        const VI4 maxHeight = Set(dix4, inputHeight - 1);
        const VI4 iZeros = Zero(dix4);
        const VF4 vfZeros = Zero(dfx4);
        const VI4 srcStrideV = Set(dix4, srcStride);
        const int mMaxWidth = inputWidth - 1;
        const int mMaxHeight = inputHeight - 1;

        for (int x = 0; x < outputWidth; ++x) {
            float srcX = (float) x * xScale;
            float srcY = (float) y * yScale;

            int x1 = static_cast<int>(srcX);
            int y1 = static_cast<int>(srcY);

            if (option == bilinear) {
                if (components == 4 && x + 8 < outputWidth) {
                    VI4 currentX = Set(dix4, x);
                    VI4 currentXV = Add(currentX, shiftV);
                    VF4 currentXVF = Mul(ConvertTo(dfx4, currentXV), xScaleV);
                    VF4 currentYVF = Mul(ConvertTo(dfx4, Set(dix4, y)), yScaleV);

                    VI4 xi1 = ConvertTo(dix4, Floor(currentXVF));
                    VI4 yi1 = Min(ConvertTo(dix4, Floor(currentYVF)), maxHeight);

                    VI4 xi2 = Min(Add(xi1, addOne), maxWidth);
                    VI4 yi2 = Min(Add(yi1, addOne), maxHeight);

                    VI4 idx = Max(Sub(xi2, xi1), iZeros);
                    VI4 idy = Max(Sub(yi2, yi1), iZeros);

                    VI4 row1Add = Mul(yi1, srcStrideV);
                    VI4 row2Add = Mul(yi2, srcStrideV);

                    VF4 dx = ConvertTo(dfx4, idx);
                    VF4 dy = ConvertTo(dfx4, idy);
                    VF4 invertDx = Sub(fOneV, dx);
                    VF4 invertDy = Sub(fOneV, dy);

                    VF4 pf1 = Mul(invertDx, invertDy);
                    VF4 pf2 = Mul(dx, invertDy);
                    VF4 pf3 = Mul(invertDx, dy);
                    VF4 pf4 = Mul(dx, dy);

                    for (int i = 0; i < 4; i++) {
                        auto row1 = reinterpret_cast<const float16_t *>(src8 +
                                                                        ExtractLane(row1Add, i));
                        auto row2 = reinterpret_cast<const float16_t *>(src8 +
                                                                        ExtractLane(row2Add, i));
                        VF16x4 lane = LoadU(df16x4, &row1[ExtractLane(xi1, i) * components]);
                        VF4 c1 = Mul(PromoteTo(dfx4, lane), pf1);
                        lane = LoadU(df16x4, &row1[ExtractLane(xi2, i) * components]);
                        VF4 c2 = Mul(PromoteTo(dfx4, lane), pf2);
                        lane = LoadU(df16x4, &row2[ExtractLane(xi1, i) * components]);
                        VF4 c3 = Mul(PromoteTo(dfx4, lane), pf3);
                        lane = LoadU(df16x4, &row2[ExtractLane(xi2, i) * components]);
                        VF4 c4 = Mul(PromoteTo(dfx4, lane), pf4);
                        VF4 sum = Max(Add(Add(Add(c1, c2), c3), c4), vfZeros);
                        VF16x4 pixel = DemoteTo(df16x4, sum);
                        auto u8Store = reinterpret_cast<float16_t *>(&dst16[
                                ExtractLane(currentXV, i) * components]);
                        StoreU(pixel, df16x4, u8Store);
                    }

                    x += components - 1;
                } else {
                    int x2 = min(x1 + 1, inputWidth - 1);
                    int y2 = min(y1 + 1, inputHeight - 1);

                    float dx((float) x2 - (float) x1);
                    float dy((float) y2 - (float) y1);

                    float invertDx = float(1.0f) - dx;
                    float invertDy = float(1.0f) - dy;

                    auto row1 = reinterpret_cast<const uint16_t *>(src8 + y1 * srcStride);
                    auto row2 = reinterpret_cast<const uint16_t *>(src8 + y2 * srcStride);

                    float pf1 = invertDx * invertDy;
                    float pf2 = dx * invertDy;
                    float pf3 = invertDx * dy;
                    float pf4 = dx * dy;

                    for (int c = 0; c < components; ++c) {
                        float c1 = castU16(row1[x1 * components + c]) * pf1;
                        float c2 = castU16(row1[x2 * components + c]) * pf2;
                        float c3 = castU16(row2[x1 * components + c]) * pf3;
                        float c4 = castU16(row2[x2 * components + c]) * pf4;

                        float result = c1 + c2 + c3 + c4;
                        dst16[x * components + c] = half(result).data_;
                    }
                }
            } else if (option == cubic || option == mitchell || option == bSpline ||
                       option == catmullRom || option == hermite) {
                if (components == 4 && x + 8 < outputWidth &&
                    (option == hermite || option == mitchell || option == catmullRom ||
                     option == bSpline || option == cubic)) {
                    VI4 currentX = Set(dix4, x);
                    VI4 currentXV = Add(currentX, shiftV);
                    VF4 currentXVF = Mul(ConvertTo(dfx4, currentXV), xScaleV);
                    VF4 currentYVF = Mul(ConvertTo(dfx4, Set(dix4, y)), yScaleV);

                    VI4 xi1 = ConvertTo(dix4, Floor(currentXVF));
                    VI4 yi1 = Min(ConvertTo(dix4, Floor(currentYVF)), maxHeight);

                    VI4 xi2 = Min(Add(xi1, addOne), maxWidth);
                    VI4 yi2 = Min(Add(yi1, addOne), maxHeight);

                    VI4 row1Add = Mul(yi1, srcStrideV);
                    VI4 row2Add = Mul(yi2, srcStrideV);

                    VF4 fdx = Sub(currentXVF, ConvertTo(dfx4, xi1));

                    for (int i = 0; i < 4; i++) {
                        auto row1 = reinterpret_cast<const float16_t *>(src8 +
                                                                        ExtractLane(row1Add, i));
                        auto row2 = reinterpret_cast<const float16_t *>(src8 +
                                                                        ExtractLane(row2Add, i));
                        VF16x4 lane = LoadU(df16x4, reinterpret_cast<const float16_t *>(&row1[
                                ExtractLane(xi1, i) * components]));
                        VF4 c1 = PromoteTo(dfx4, lane);
                        lane = LoadU(df16x4,
                                     reinterpret_cast<const float16_t *>(&row1[ExtractLane(xi2, i) *
                                                                               components]));
                        VF4 c2 = PromoteTo(dfx4, lane);
                        lane = LoadU(df16x4,
                                     reinterpret_cast<const float16_t *>(&row2[ExtractLane(xi1, i) *
                                                                               components]));
                        VF4 c3 = PromoteTo(dfx4, lane);
                        lane = LoadU(df16x4,
                                     reinterpret_cast<const float16_t *>(&row2[ExtractLane(xi2, i) *
                                                                               components]));
                        VF4 c4 = PromoteTo(dfx4, lane);

                        VF4 pixels;
                        if (option == hermite) {
                            pixels = CubicHermiteV(dfx4, fdx, c1, c2, c3, c4);
                        } else if (option == mitchell) {
                            pixels = MitchellNetravaliV(dfx4, fdx, c1, c2, c3, c4);
                        } else if (option == catmullRom) {
                            pixels = CatmullRomV(dfx4, fdx, c1, c2, c3, c4);
                        } else if (option == bSpline) {
                            pixels = CubicBSplineV(dfx4, fdx, c1, c2, c3, c4);
                        } else {
                            pixels = SimpleCubicV(dfx4, fdx, c1, c2, c3, c4);
                        }

                        VF16x4 pixel = DemoteTo(df16x4, pixels);

                        auto u16Store = reinterpret_cast<float16_t *>(&dst16[
                                ExtractLane(currentXV, i) * components]);
                        StoreU(pixel, df16x4, u16Store);
                    }

                    x += components - 1;
                } else {
                    KernelSample4Func sampler;
                    switch (option) {
                        case cubic:
                            sampler = SimpleCubic<float>;
                            break;
                        case mitchell:
                            sampler = MitchellNetravali<float>;
                            break;
                        case catmullRom:
                            sampler = CatmullRom<float>;
                            break;
                        case bSpline:
                            sampler = CubicBSpline<float>;
                            break;
                        case hermite:
                            sampler = CubicHermite<float>;
                            break;
                        default:
                            sampler = CubicBSpline<float>;
                    }
                    float kx1 = floor(srcX);
                    float ky1 = floor(srcY);

                    int xi = (int) kx1;
                    int yj = (int) ky1;

                    auto row = reinterpret_cast<const uint16_t *>(src8 +
                                                                  clamp(yj, 0, inputHeight - 1) *
                                                                  srcStride);
                    auto rowy1 = reinterpret_cast<const uint16_t *>(src8 +
                                                                    clamp(yj + 1, 0,
                                                                          inputHeight - 1) *
                                                                    srcStride);

                    for (int c = 0; c < components; ++c) {
                        float clr = sampler(srcX - (float) xi,
                                            (float) castU16(
                                                    row[clamp(xi, 0, inputWidth - 1) * components +
                                                        c]),
                                            (float) castU16(
                                                    rowy1[clamp(xi, 0, inputWidth - 1) *
                                                          components +
                                                          c]),
                                            (float) castU16(
                                                    row[clamp(xi + 1, 0, inputWidth - 1) *
                                                        components +
                                                        c]),
                                            (float) castU16(
                                                    rowy1[clamp(xi + 1, 0, inputWidth - 1) *
                                                          components +
                                                          c]));
                        dst16[x * components + c] = half(clr).data_;
                    }
                }
            } else if (option == lanczos || option == hann) {
                if (x + 8 < outputWidth && components == 4) {
                    KernelWindow2Func sampler;
                    auto lanczosFA = float(3.0f);
                    int a = 3;
                    switch (option) {
                        case hann:
                            sampler = HannWindow<float>;
                            break;
                        default:
                            sampler = LanczosWindow<float>;
                    }
                    float rgb[components];
                    fill(rgb, rgb + components, 0.0f);

                    float kx1 = floor(srcX);
                    float ky1 = floor(srcY);

                    float kWeightSum = 0;
                    VF4 color = Set(dfx4, 0);

                    const int appendixLow[4] = {-2, -1, 0, 1};
                    const int appendixHigh[4] = {2, 3, 0, 0};

                    const VF4 aVector = Set(dfx4, a);
                    VF4 srcXV = Set(dfx4, srcX);
                    VI4 kx1V = Set(dix4, kx1);
                    const VI4 appendixLowV = LoadU(dix4, appendixLow);
                    const VI4 appendixHighV = LoadU(dix4, appendixHigh);

                    for (int j = -a + 1; j <= a; j++) {
                        int yj = (int) ky1 + j;
                        float dy = float(srcY) - (float(ky1) + (float) j);
                        float yWeight;
                        if (option == lanczos) {
                            yWeight = LanczosWindow(dy, lanczosFA);
                        } else {
                            yWeight = HannWindow(float(j), lanczosFA);
                        }
                        auto row = reinterpret_cast<const uint16_t *>(src8 +
                                                                      clamp(yj, 0,
                                                                            inputHeight - 1) *
                                                                      srcStride);
                        VF4 yWeightV = Set(dfx4, yWeight);
                        VI4 xi = Add(kx1V, appendixLowV);
                        VF4 dx = Sub(srcXV, ConvertTo(dfx4, xi));
                        VF4 weights;
                        if (option == lanczos) {
                            weights = Mul(LanczosWindowHWY(dfx4, dx, aVector), yWeightV);
                        } else {
                            weights = Mul(HannWindow(dfx4, ConvertTo(dfx4, appendixLowV), a),
                                          yWeightV);
                        }
                        kWeightSum += ExtractLane(SumOfLanes(dfx4, weights), 0);
                        for (int i = 0; i < 4; ++i) {
                            int sizeXPos = clamp(ExtractLane(xi, i), 0, mMaxWidth) * components;
                            VF16x4 r1 = LoadU(df16x4,
                                              reinterpret_cast<const float16_t *>(&row[sizeXPos]));
                            VF4 fr1 = PromoteTo(dfx4, r1);
                            fr1 = Mul(fr1, Set(dfx4, ExtractLane(weights, i)));
                            color = Add(color, fr1);
                        }

                        xi = Add(kx1V, appendixHighV);
                        dx = Sub(srcXV, ConvertTo(dfx4, xi));
                        if (option == lanczos) {
                            weights = Mul(LanczosWindowHWY(dfx4, dx, aVector), yWeightV);
                        } else {
                            weights = Mul(HannWindow(dfx4, ConvertTo(dfx4, appendixHighV), a),
                                          yWeightV);
                        }
                        for (int i = 0; i < 2; ++i) {
                            int sizeXPos = clamp(ExtractLane(xi, i), 0, mMaxWidth) * components;
                            VF16x4 r1 = LoadU(df16x4,
                                              reinterpret_cast<const float16_t *>(&row[sizeXPos]));
                            VF4 fr1 = PromoteTo(dfx4, r1);
                            float weight = ExtractLane(weights, i);
                            kWeightSum += weight;
                            fr1 = Mul(fr1, Set(dfx4, weight));
                            color = Add(color, fr1);
                        }
                    }

                    if (kWeightSum == 0) {
                        VF16x4 f16Color = DemoteTo(df16x4, color);
                        StoreU(f16Color, df16x4,
                               reinterpret_cast<float16_t *>(&dst16[x * components]));
                    } else {
                        VF16x4 f16Color = DemoteTo(df16x4, Div(color, Set(dfx4, kWeightSum)));
                        StoreU(f16Color, df16x4,
                               reinterpret_cast<float16_t *>(&dst16[x * components]));
                    }
                } else {
                    auto lanczosFA = float(3.0f);
                    int a = 3;
                    float rgb[components];
                    fill(rgb, rgb + components, 0.0f);

                    float kx1 = floor(srcX);
                    float ky1 = floor(srcY);

                    float weightSum(0.0f);

                    for (int j = -a + 1; j <= a; j++) {
                        int yj = (int) ky1 + j;
                        float dy = float(srcY) - (float(ky1) + (float) j);
                        float yWeight;
                        if (option == lanczos) {
                            yWeight = LanczosWindow(dy, lanczosFA);
                        } else {
                            yWeight = HannWindow(float(j), lanczosFA);
                        }
                        for (int i = -a + 1; i <= a; i++) {
                            int xi = (int) kx1 + i;
                            float dx = float(srcX) - (float(kx1) + (float) i);
                            float weight;
                            if (option == lanczos) {
                                weight = LanczosWindow(dx, lanczosFA) * yWeight;
                            } else {
                                weight = HannWindow(float(i), lanczosFA) * yWeight;
                            }
                            weightSum += weight;

                            auto row = reinterpret_cast<const uint16_t *>(src8 +
                                                                          clamp(yj, 0,
                                                                                inputHeight - 1) *
                                                                          srcStride);

                            for (int c = 0; c < components; ++c) {
                                half clrf = castU16(
                                        row[clamp(xi, 0, inputWidth - 1) * components + c]);
                                float clr = (float) clrf * weight;
                                rgb[c] += clr;
                            }
                        }
                    }

                    for (int c = 0; c < components; ++c) {
                        if (weightSum == 0) {
                            dst16[x * components + c] = half(rgb[c]).data_;
                        } else {
                            dst16[x * components + c] = half(rgb[c] / weightSum).data_;
                        }
                    }
                }
            } else {
                auto row = reinterpret_cast<const uint16_t *>(src8 + y1 * srcStride);
                memcpy(&dst16[x * components], &row[x1 * components],
                       sizeof(uint16_t) * components);
            }
        }
    }

    void scaleImageFloat16HWY(const uint16_t *input,
                              int srcStride,
                              int inputWidth, int inputHeight,
                              uint16_t *output,
                              int dstStride,
                              int outputWidth, int outputHeight,
                              int components,
                              XSampler option) {
        float xScale = static_cast<float>(inputWidth) / static_cast<float>(outputWidth);
        float yScale = static_cast<float>(inputHeight) / static_cast<float>(outputHeight);

        auto src8 = reinterpret_cast<const uint8_t *>(input);

        int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                    outputHeight * outputWidth / (256 * 256)), 1, 12);
        std::vector<std::thread> workers;

        int segmentHeight = outputHeight / threadCount;

        for (int i = 0; i < threadCount; i++) {
            int start = i * segmentHeight;
            int end = (i + 1) * segmentHeight;
            if (i == threadCount - 1) {
                end = outputHeight;
            }
            workers.emplace_back([start, end, src8, srcStride, dstStride, inputWidth, inputHeight,
                                         output, outputWidth,
                                         xScale, option, yScale, components]() {
                for (int y = start; y < end; ++y) {
                    scaleRowF16(src8, srcStride, dstStride, inputWidth, inputHeight,
                                output, outputWidth,
                                xScale, option, yScale, y, components);
                }
            });
        }

        for (std::thread &thread: workers) {
            thread.join();
        }
    }

    void
    ScaleRowU8(const uint8_t *src8,
               const int srcStride,
               int inputWidth, int inputHeight, uint8_t *output,
               const int dstStride,
               int outputWidth,
               const int components,
               const XSampler option,
               float xScale,
               float yScale,
               float maxColors,
               int y) {
        auto dst8 = reinterpret_cast<uint8_t *>(output + y * dstStride);
        auto dst = reinterpret_cast<uint8_t *>(dst8);

        const FixedTag<float32_t, 4> dfx4;
        const FixedTag<int32_t, 4> dix4;
        const FixedTag<uint32_t, 4> dux4;
        const FixedTag<uint8_t, 4> du8x4;
        using VI4 = Vec<decltype(dix4)>;
        using VF4 = Vec<decltype(dfx4)>;
        using VU8x4 = Vec<decltype(du8x4)>;

        const int shift[4] = {0, 1, 2, 3};
        const VI4 shiftV = LoadU(dix4, shift);
        const VF4 xScaleV = Set(dfx4, xScale);
        const VF4 yScaleV = Set(dfx4, yScale);
        const VI4 addOne = Set(dix4, 1);
        const VF4 fOneV = Set(dfx4, 1.0f);
        const VI4 maxWidth = Set(dix4, inputWidth - 1);
        const VI4 maxHeight = Set(dix4, inputHeight - 1);
        const VI4 iZeros = Zero(dix4);
        const VF4 vfZeros = Zero(dfx4);
        const VI4 srcStrideV = Set(dix4, srcStride);
        const VF4 maxColorsV = Set(dfx4, maxColors);

        for (int x = 0; x < outputWidth; ++x) {
            float srcX = (float) x * xScale;
            float srcY = (float) y * yScale;

            int x1 = static_cast<int>(srcX);
            int y1 = static_cast<int>(srcY);

            if (option == bilinear) {
                if (components == 4 && x + 8 < outputWidth) {
                    VI4 currentX = Set(dix4, x);
                    VI4 currentXV = Add(currentX, shiftV);
                    VF4 currentXVF = Mul(ConvertTo(dfx4, currentXV), xScaleV);
                    VF4 currentYVF = Mul(ConvertTo(dfx4, Set(dix4, y)), yScaleV);

                    VI4 xi1 = ConvertTo(dix4, Floor(currentXVF));
                    VI4 yi1 = Min(ConvertTo(dix4, Floor(currentYVF)), maxHeight);

                    VI4 xi2 = Min(Add(xi1, addOne), maxWidth);
                    VI4 yi2 = Min(Add(yi1, addOne), maxHeight);

                    VI4 idx = Max(Sub(xi2, xi1), iZeros);
                    VI4 idy = Max(Sub(yi2, yi1), iZeros);

                    VI4 row1Add = Mul(yi1, srcStrideV);
                    VI4 row2Add = Mul(yi2, srcStrideV);

                    VF4 dx = ConvertTo(dfx4, idx);
                    VF4 dy = ConvertTo(dfx4, idy);
                    VF4 invertDx = Sub(fOneV, dx);
                    VF4 invertDy = Sub(fOneV, dy);

                    VF4 pf1 = Mul(invertDx, invertDy);
                    VF4 pf2 = Mul(dx, invertDy);
                    VF4 pf3 = Mul(invertDx, dy);
                    VF4 pf4 = Mul(dx, dy);

                    for (int i = 0; i < 4; i++) {
                        auto row1 = reinterpret_cast<const uint8_t *>(src8 +
                                                                      ExtractLane(row1Add, i));
                        auto row2 = reinterpret_cast<const uint8_t *>(src8 +
                                                                      ExtractLane(row2Add, i));

                        VU8x4 lane = LoadU(du8x4, reinterpret_cast<const uint8_t *>(&row1[
                                ExtractLane(xi1, i) * components]));
                        VF4 c1 = Mul(ConvertTo(dfx4, PromoteTo(dux4, lane)), pf1);
                        lane = LoadU(du8x4,
                                     reinterpret_cast<const uint8_t *>(&row1[ExtractLane(xi2, i) *
                                                                             components]));
                        VF4 c2 = Mul(ConvertTo(dfx4, PromoteTo(dux4, lane)), pf2);
                        lane = LoadU(du8x4,
                                     reinterpret_cast<const uint8_t *>(&row2[ExtractLane(xi1, i) *
                                                                             components]));
                        VF4 c3 = Mul(ConvertTo(dfx4, PromoteTo(dux4, lane)), pf3);
                        lane = LoadU(du8x4,
                                     reinterpret_cast<const uint8_t *>(&row2[ExtractLane(xi2, i) *
                                                                             components]));
                        VF4 c4 = Mul(ConvertTo(dfx4, PromoteTo(dux4, lane)), pf4);
                        VF4 sum = Max(Min(Round(Add(Add(Add(c1, c2), c3), c4)), maxColorsV),
                                      vfZeros);
                        VU8x4 pixel = DemoteTo(du8x4, ConvertTo(dux4, sum));
                        auto u8Store = &dst[ExtractLane(currentXV, i) * components];
                        StoreU(pixel, du8x4, u8Store);
                    }

                    x += components - 1;
                } else {
                    for (int c = 0; c < components; ++c) {
                        int x2 = min(x1 + 1, inputWidth - 1);
                        int y2 = min(y1 + 1, inputHeight - 1);

                        float dx = max((float) x2 - (float) x1, 0.0f);
                        float dy = max((float) y2 - (float) y1, 0.0f);

                        auto row1 = reinterpret_cast<const uint8_t *>(src8 + y1 * srcStride);
                        auto row2 = reinterpret_cast<const uint8_t *>(src8 + y2 * srcStride);

                        float invertDx = float(1.0f) - dx;
                        float invertDy = float(1.0f) - dy;

                        float pf1 = invertDx * invertDy;
                        float pf2 = dx * invertDy;
                        float pf3 = invertDx * dy;
                        float pf4 = dx * dy;
                        float c1 = static_cast<float>(row1[x1 * components + c]) * pf1;
                        float c2 = static_cast<float>(row1[x2 * components + c]) * pf2;
                        float c3 = static_cast<float>(row2[x1 * components + c]) * pf3;
                        float c4 = static_cast<float>(row2[x2 * components + c]) * pf4;

                        float result = (c1 + c2 + c3 + c4);
                        float f = result;
                        f = clamp(round(f), 0.0f, maxColors);
                        dst[x * components + c] = static_cast<uint8_t>(f);
                    }
                }
            } else if (option == cubic || option == mitchell || option == bSpline ||
                       option == catmullRom || option == hermite) {
                if (components == 4 && x + 8 < outputWidth &&
                    (option == hermite || option == mitchell || option == catmullRom ||
                     option == bSpline || option == cubic)) {
                    VI4 currentX = Set(dix4, x);
                    VI4 currentXV = Add(currentX, shiftV);
                    VF4 currentXVF = Mul(ConvertTo(dfx4, currentXV), xScaleV);
                    VF4 currentYVF = Mul(ConvertTo(dfx4, Set(dix4, y)), yScaleV);

                    VI4 xi1 = ConvertTo(dix4, Floor(currentXVF));
                    VI4 yi1 = Min(ConvertTo(dix4, Floor(currentYVF)), maxHeight);

                    VI4 xi2 = Min(Add(xi1, addOne), maxWidth);
                    VI4 yi2 = Min(Add(yi1, addOne), maxHeight);

                    VI4 row1Add = Mul(yi1, srcStrideV);
                    VI4 row2Add = Mul(yi2, srcStrideV);

                    VF4 fdx = Sub(currentXVF, ConvertTo(dfx4, xi1));

                    for (int i = 0; i < 4; i++) {
                        auto row1 = reinterpret_cast<const uint8_t *>(src8 +
                                                                      ExtractLane(row1Add, i));
                        auto row2 = reinterpret_cast<const uint8_t *>(src8 +
                                                                      ExtractLane(row2Add, i));
                        VU8x4 lane = LoadU(du8x4, reinterpret_cast<const uint8_t *>(&row1[
                                ExtractLane(xi1, i) * components]));
                        VF4 c1 = ConvertTo(dfx4, PromoteTo(dux4, lane));
                        lane = LoadU(du8x4,
                                     reinterpret_cast<const uint8_t *>(&row1[ExtractLane(xi2, i) *
                                                                             components]));
                        VF4 c2 = ConvertTo(dfx4, PromoteTo(dux4, lane));
                        lane = LoadU(du8x4,
                                     reinterpret_cast<const uint8_t *>(&row2[ExtractLane(xi1, i) *
                                                                             components]));
                        VF4 c3 = ConvertTo(dfx4, PromoteTo(dux4, lane));
                        lane = LoadU(du8x4,
                                     reinterpret_cast<const uint8_t *>(&row2[ExtractLane(xi2, i) *
                                                                             components]));
                        VF4 c4 = ConvertTo(dfx4, PromoteTo(dux4, lane));

                        VF4 pixels;
                        if (option == hermite) {
                            pixels = CubicHermiteV(dfx4, fdx, c1, c2, c3, c4);
                        } else if (option == mitchell) {
                            pixels = MitchellNetravaliV(dfx4, fdx, c1, c2, c3, c4);
                        } else if (option == catmullRom) {
                            pixels = CatmullRomV(dfx4, fdx, c1, c2, c3, c4);
                        } else if (option == bSpline) {
                            pixels = CubicBSplineV(dfx4, fdx, c1, c2, c3, c4);
                        } else {
                            pixels = SimpleCubicV(dfx4, fdx, c1, c2, c3, c4);
                        }

                        pixels = Max(Min(Round(pixels), maxColorsV), vfZeros);

                        VU8x4 pixel = DemoteTo(du8x4, ConvertTo(dux4, pixels));

                        auto u8Store = reinterpret_cast<uint8_t *>(&dst[
                                ExtractLane(currentXV, i) * components]);
                        StoreU(pixel, du8x4, u8Store);
                    }

                    x += components - 1;
                } else {
                    KernelSample4Func sampler;
                    switch (option) {
                        case cubic:
                            sampler = SimpleCubic<float>;
                            break;
                        case mitchell:
                            sampler = MitchellNetravali<float>;
                            break;
                        case catmullRom:
                            sampler = CatmullRom<float>;
                            break;
                        case bSpline:
                            sampler = CubicBSpline<float>;
                            break;
                        case hermite:
                            sampler = CubicHermite<float>;
                            break;
                        default:
                            sampler = CubicBSpline<float>;
                    }
                    float kx1 = floor(srcX);
                    float ky1 = floor(srcY);

                    int xi = (int) kx1;
                    int yj = (int) ky1;

                    auto row = reinterpret_cast<const uint8_t *>(src8 +
                                                                 clamp(yj, 0, inputHeight - 1) *
                                                                 srcStride);
                    auto rowy1 = reinterpret_cast<const uint8_t *>(src8 +
                                                                   clamp(yj + 1, 0,
                                                                         inputHeight - 1) *
                                                                   srcStride);

                    for (int c = 0; c < components; ++c) {
                        float weight = sampler(srcX - (float) xi,
                                               static_cast<float>(row[
                                                       clamp(xi, 0, inputWidth - 1) * components +
                                                       c]),
                                               static_cast<float>(rowy1[
                                                       clamp(xi, 0, inputWidth - 1) *
                                                       components + c]),
                                               static_cast<float>(row[
                                                       clamp(xi + 1, 0, inputWidth - 1) *
                                                       components + c]),
                                               static_cast<float>(rowy1[
                                                       clamp(xi + 1, 0, inputWidth - 1) *
                                                       components + c]));
                        dst[x * components + c] = static_cast<uint8_t>(clamp(round(weight), 0.0f,
                                                                             maxColors));
                    }
                }
            } else if (option == lanczos || option == hann) {
                if (x + 8 < outputWidth && components == 4) {
                    KernelWindow2Func sampler;
                    switch (option) {
                        case hann:
                            sampler = HannWindow<float>;
                            break;
                        default:
                            sampler = LanczosWindow<float>;
                    }
                    // only kernel with size 3 is supported
                    constexpr int kernelSize = 3;

                    float kx1 = floor(srcX);
                    float ky1 = floor(srcY);

                    float kWeightSum = 0;
                    VF4 color = Set(dfx4, 0);

                    const int a = kernelSize;
                    const int mMaxHeight = inputHeight - 1;
                    const int mMaxWidth = inputWidth - 1;

                    const int appendixLow[4] = {-2, -1, 0, 1};
                    const int appendixHigh[4] = {2, 3, 0, 0};

                    const VF4 aVector = Set(dfx4, a);
                    VF4 srcXV = Set(dfx4, srcX);
                    VI4 kx1V = Set(dix4, kx1);
                    const VI4 appendixLowV = LoadU(dix4, appendixLow);
                    const VI4 appendixHighV = LoadU(dix4, appendixHigh);

                    for (int j = -a + 1; j <= a; j++) {
                        int yj = (int) ky1 + j;
                        float dy = float(srcY) - (float(ky1) + (float) j);
                        float yWeight;
                        if (option == lanczos) {
                            yWeight = sampler(dy, float(kernelSize));
                        } else {
                            yWeight = HannWindow(float(j), float(kernelSize));
                        }
                        auto row = reinterpret_cast<const uint8_t *>(src8 +
                                                                     clamp(yj, 0,
                                                                           inputHeight - 1) *
                                                                     srcStride);
                        VF4 yWeightV = Set(dfx4, yWeight);
                        VI4 xi = Add(kx1V, appendixLowV);
                        VF4 dx = Sub(srcXV, ConvertTo(dfx4, xi));
                        VF4 weights;
                        if (option == lanczos) {
                            weights = Mul(LanczosWindowHWY(dfx4, dx, aVector), yWeightV);
                        } else {
                            weights = Mul(HannWindow(dfx4, ConvertTo(dfx4, appendixLowV), a),
                                          yWeightV);
                        }
                        kWeightSum += ExtractLane(SumOfLanes(dfx4, weights), 0);
                        for (int i = 0; i < 4; ++i) {
                            int sizeXPos = clamp(ExtractLane(xi, i), 0, mMaxWidth) * components;
                            VU8x4 u81 = LoadU(du8x4,
                                              reinterpret_cast<const uint8_t *>(&row[sizeXPos]));
                            VF4 fr1 = ConvertTo(dfx4, PromoteTo(dix4, u81));
                            fr1 = Mul(fr1, Set(dfx4, ExtractLane(weights, i)));
                            color = Add(color, fr1);
                        }

                        xi = Add(kx1V, appendixHighV);
                        dx = Sub(srcXV, ConvertTo(dfx4, xi));
                        if (option == lanczos) {
                            weights = Mul(LanczosWindowHWY(dfx4, dx, aVector), yWeightV);
                        } else {
                            weights = Mul(HannWindow(dfx4, ConvertTo(dfx4, appendixHighV), a),
                                          yWeightV);
                        }
                        for (int i = 0; i < 2; ++i) {
                            int sizeXPos = clamp(ExtractLane(xi, i), 0, mMaxWidth) * components;
                            VU8x4 u81 = LoadU(du8x4,
                                              reinterpret_cast<const uint8_t *>(&row[sizeXPos]));
                            VF4 fr1 = ConvertTo(dfx4, PromoteTo(dix4, u81));
                            float weight = ExtractLane(weights, i);
                            kWeightSum += weight;
                            fr1 = Mul(fr1, Set(dfx4, weight));
                            color = Add(color, fr1);
                        }
                    }

                    if (kWeightSum == 0) {
                        color = Max(Min(color, maxColorsV), vfZeros);
                        VU8x4 u8Color = DemoteTo(du8x4, ConvertTo(dux4, color));
                        StoreU(u8Color, du8x4,
                               reinterpret_cast<uint8_t *>(&dst[x * components]));
                    } else {
                        color = Max(Min(Div(color, Set(dfx4, kWeightSum)), maxColorsV),
                                    vfZeros);
                        VU8x4 u8Color = DemoteTo(du8x4, ConvertTo(dux4, color));
                        StoreU(u8Color, du8x4,
                               reinterpret_cast<uint8_t *>(&dst[x * components]));
                    }
                } else {
                    KernelWindow2Func sampler;
                    auto lanczosFA = float(3.0f);
                    int a = 3;
                    switch (option) {
                        case hann:
                            sampler = HannWindow<float>;
                            a = 3;
                            lanczosFA = 3;
                            break;
                        default:
                            sampler = LanczosWindow<float>;
                    }
                    float rgb[components];
                    fill(rgb, rgb + components, 0.0f);

                    float kx1 = floor(srcX);
                    float ky1 = floor(srcY);

                    float weightSum(0.0f);

                    for (int j = -a + 1; j <= a; j++) {
                        int yj = (int) ky1 + j;
                        float dy = float(srcY) - (float(ky1) + (float) j);
                        float yWeight;
                        if (option == lanczos) {
                            yWeight = LanczosWindow(dy, float(lanczosFA));
                        } else {
                            yWeight = HannWindow(float(j), float(lanczosFA));
                        }
                        for (int i = -a + 1; i <= a; i++) {
                            int xi = (int) kx1 + i;
                            float dx = float(srcX) - (float(kx1) + (float) i);
                            float weight;
                            if (option == lanczos) {
                                weight = LanczosWindow(dx, float(lanczosFA)) * yWeight;
                            } else {
                                weight = HannWindow(float(i), float(lanczosFA)) * yWeight;
                            }
                            weightSum += weight;

                            auto row = reinterpret_cast<const uint8_t *>(src8 +
                                                                         clamp(yj, 0,
                                                                               inputHeight - 1) *
                                                                         srcStride);

                            for (int c = 0; c < components; ++c) {
                                auto clrf = static_cast<float>(row[
                                        clamp(xi, 0, inputWidth - 1) * components + c]);
                                float clr = clrf * weight;
                                rgb[c] += clr;
                            }
                        }
                    }

                    for (int c = 0; c < components; ++c) {
                        if (weightSum == 0) {
                            dst[x * components + c] = static_cast<uint8_t>(clamp(round(rgb[c]),
                                                                                 0.0f,
                                                                                 maxColors));
                        } else {
                            dst[x * components + c] = static_cast<uint8_t>(clamp(
                                    round(rgb[c] / weightSum),
                                    0.0f, maxColors));
                        }
                    }
                }
            } else {
                if (components == 4) {
                    auto row = reinterpret_cast<const uint8_t *>(src8 + y1 * srcStride);
                    reinterpret_cast<uint32_t *>(dst)[x] = reinterpret_cast<const uint32_t *>(row)[x1];
                } else {
                    auto row = reinterpret_cast<const uint8_t *>(src8 + y1 * srcStride);
                    memcpy(&dst[x * components], &row[x1 * components],
                           sizeof(uint8_t) * components);
                }
            }
        }
    }

    void scaleImageU8HWY(const uint8_t *input,
                         const int srcStride,
                         int inputWidth,
                         int inputHeight,
                         uint8_t *output,
                         const int dstStride,
                         int outputWidth,
                         int outputHeight,
                         const int components,
                         const int depth,
                         const XSampler option) {
        float xScale = static_cast<float>(inputWidth) / static_cast<float>(outputWidth);
        float yScale = static_cast<float>(inputHeight) / static_cast<float>(outputHeight);

        auto src8 = reinterpret_cast<const uint8_t *>(input);

        float maxColors = pow(2.0f, (float) depth) - 1.0f;

        int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                    outputHeight * outputWidth / (256 * 256)), 1, 12);
        vector<std::thread> workers;

        int segmentHeight = outputHeight / threadCount;

        for (int i = 0; i < threadCount; i++) {
            int start = i * segmentHeight;
            int end = (i + 1) * segmentHeight;
            if (i == threadCount - 1) {
                end = outputHeight;
            }
            workers.emplace_back([start, end, src8, srcStride, inputWidth, inputHeight, output,
                                         dstStride, outputWidth, components,
                                         option,
                                         xScale, yScale, maxColors]() {
                for (int y = start; y < end; ++y) {
                    ScaleRowU8(src8, srcStride, inputWidth, inputHeight, output,
                               dstStride, outputWidth, components,
                               option,
                               xScale, yScale, maxColors, y);
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
    HWY_EXPORT(scaleImageFloat16HWY);

    void scaleImageFloat16(const uint16_t *input,
                           int srcStride,
                           int inputWidth, int inputHeight,
                           uint16_t *output,
                           int dstStride,
                           int outputWidth, int outputHeight,
                           int components,
                           XSampler option) {
        HWY_DYNAMIC_DISPATCH(scaleImageFloat16HWY)(input, srcStride, inputWidth, inputHeight,
                                                   output, dstStride, outputWidth, outputHeight,
                                                   components, option);
    }

    HWY_EXPORT(scaleImageU8HWY);

    void scaleImageU8(const uint8_t *input,
                      int srcStride,
                      int inputWidth, int inputHeight,
                      uint8_t *output,
                      int dstStride,
                      int outputWidth, int outputHeight,
                      int components,
                      int depth,
                      XSampler option) {
        HWY_DYNAMIC_DISPATCH(scaleImageU8HWY)(input, srcStride, inputWidth, inputHeight, output,
                                              dstStride, outputWidth, outputHeight, components,
                                              depth, option);
    }
}
#endif