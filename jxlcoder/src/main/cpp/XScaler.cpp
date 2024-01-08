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

#if defined(__clang__)
#pragma clang fp contract(fast) exceptions(ignore) reassociate(on)
#endif

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "XScaler.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

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

    // P Found using maxima
//
// y(x) := 4 * x * (%pi-x) / (%pi^2) ;
// z(x) := (1-p)*y(x) + p * y(x)^2;
// e(x) := z(x) - sin(x);
// solve( diff( integrate( e(x)^2, x, 0, %pi/2 ), p ) = 0, p ),numer;
//
// [p = .2248391013559941]
    template<typename T>
    inline T fastSin1(T x) {
        constexpr T A = T(4.0) / (T(M_PI) * T(M_PI));
        constexpr T P = 0.2248391013559941;
        T y = A * x * (T(M_PI) - x);
        return y * ((1 - P) + y * P);
    }

// P and Q found using maxima
//
// y(x) := 4 * x * (%pi-x) / (%pi^2) ;
// zz(x) := (1-p-q)*y(x) + p * y(x)^2 + q * y(x)^3
// ee(x) := zz(x) - sin(x)
// solve( [ integrate( diff(ee(x)^2, p ), x, 0, %pi/2 ) = 0, integrate( diff(ee(x)^2,q), x, 0, %pi/2 ) = 0 ] , [p,q] ),numer;
//
// [[p = .1952403377008734, q = .01915214119105392]]
    template<typename T>
    inline T fastSin2(T x) {
        constexpr T A = T(4.0) / (T(M_PI) * T(M_PI));
        constexpr T P = 0.1952403377008734;
        constexpr T Q = 0.01915214119105392;

        T y = A * x * (T(M_PI) - x);

        return y * ((1 - P - Q) + y * (P + y * Q));
    }

    inline half_float::half castU16(uint16_t t) {
        half_float::half result;
        result.data_ = t;
        return result;
    }

    template<typename T>
    inline half_float::half PromoteToHalf(T t, float maxColors) {
        half_float::half result((float) t / maxColors);
        return result;
    }

    template<typename D, typename T>
    inline D PromoteTo(T t, float maxColors) {
        D result = static_cast<D>((float) t / maxColors);
        return result;
    }

    template<typename T>
    inline T DemoteHalfTo(half t, float maxColors) {
        return (T) clamp(((float) t * (float) maxColors), 0.0f, (float) maxColors);
    }

    template<typename D, typename T>
    inline D DemoteTo(T t, float maxColors) {
        return (D) clamp(((float) t * (float) maxColors), 0.0f, (float) maxColors);
    }

    template<typename T>
    T SimpleCubic(T t, T A, T B, T C, T D) {
        T duplet = t * t;
        T triplet = duplet * t;
        T a = -A / T(2.0) + (T(3.0) * B) / T(2.0) - (T(3.0) * C) / T(2.0) + D / T(2.0);
        T b = A - (T(5.0) * B) / T(2.0) + T(2.0) * C - D / T(2.0);
        T c = -A / T(2.0) + C / T(2.0);
        T d = B;
        return a * triplet * T(3.0) + b * duplet + c * t + d;
    }

    template<typename T>
    T CubicHermite(T d, T p0, T p1, T p2, T p3) {
        constexpr T C = T(0.0);
        constexpr T B = T(0.0);
        T duplet = d * d;
        T triplet = duplet * d;
        T firstRow = ((T(-1 / 6.0) * B - C) * p0 + (T(-1.5) * B - C + T(2.0)) * p1 +
                      (T(1.5) * B + C - T(2.0)) * p2 + (T(1 / 6.0) * B + C) * p3) * triplet;
        T secondRow = ((T(0.5) * B + 2 * C) * p0 + (T(2.0) * B + C - T(3.0)) * p1 +
                       (T(-2.5) * B - T(2.0) * C + T(3.0)) * p2 - C * p3) * duplet;
        T thirdRow = ((T(-0.5) * B - C) * p0 + (T(0.5) * B + C) * p2) * d;
        T fourthRow =
                (T(1.0 / 6.0) * B) * p0 + (T(-1.0 / 3.0) * B + T(1)) * p1 + (T(1.0 / 6.0) * B) * p2;
        return firstRow + secondRow + thirdRow + fourthRow;
    }

    template<class D, typename T = Vec<D>>
    inline T CubicSplineGeneric(const D df, T C, T B, T d, T p0, T p1, T p2, T p3) {
        T duplet = Mul(d, d);
        T triplet = Mul(duplet, d);
        T firstRow = MulAdd(MulSub(Set(df, -1 / 6.0), B, C), p0,
                            MulAdd(Add(MulSub(Set(df, -1.5), B, C), Set(df, 2.0)), p1,
                                   MulAdd(Sub(MulAdd(Set(df, 1.5), B, C), Set(df, 2.0)), p2,
                                          Mul(MulAdd(Set(df, 1 / 6.0), B, C), p3))));
        firstRow = Mul(firstRow, triplet);
        T sc1 = MulAdd(Set(df, 0.5), B, Mul(Set(df, 2.0), C));
        T sc2 = Sub(MulAdd(Set(df, 2.0), B, C), Set(df, 3.0));
        T sc3 = Add(MulAdd(Set(df, -2.5), B, Mul(Set(df, -2.0), C)), Set(df, 3.0));
        T secondRow = MulAdd(sc1, p0, MulAdd(sc2, p1, MulAdd(sc3, p2, Mul(Neg(C), p3))));
        secondRow = Mul(secondRow, duplet);
        T thirdRow = Mul(
                MulAdd(MulSub(Set(df, -0.5), B, C), p0, Mul(MulAdd(Set(df, 0.5), B, C), p2)), d);
        T f1 = MulAdd(Mul(Set(df, 1.0 / 6.0), B), p0,
                      Mul(MulAdd(Set(df, -1.0 / 3.0), B, Set(df, 1.0)), p1));
        T f2 = Mul(Mul(Set(df, 1.0 / 6.0), B), p2);
        T fourthRow = Add(f1, f2);
        return Add(Add(Add(firstRow, secondRow), thirdRow), fourthRow);
    }

    template<class D, typename T = Vec<D>>
    inline T CubicHermiteV(const D df, T d, T p0, T p1, T p2, T p3) {
        const T C = Set(df, 0.0);
        const T B = Set(df, 0.0);
        return CubicSplineGeneric<D, T>(df, C, B, d, p0, p1, p2, p3);
    }

    template<typename T>
    T CubicBSpline(T d, T p0, T p1, T p2, T p3) {
        constexpr T C = T(0.0);
        constexpr T B = T(1.0);
        T duplet = d * d;
        T triplet = duplet * d;
        T firstRow = ((T(-1 / 6.0) * B - C) * p0 + (T(-1.5) * B - C + T(2.0)) * p1 +
                      (T(1.5) * B + C - T(2.0)) * p2 + (T(1 / 6.0) * B + C) * p3) * triplet;
        T secondRow = ((T(0.5) * B + 2 * C) * p0 + (T(2.0) * B + C - T(3.0)) * p1 +
                       (T(-2.5) * B - T(2.0) * C + T(3.0)) * p2 - C * p3) * duplet;
        T thirdRow = ((T(-0.5) * B - C) * p0 + (T(0.5) * B + C) * p2) * d;
        T fourthRow =
                (T(1.0 / 6.0) * B) * p0 + (T(-1.0 / 3.0) * B + T(1)) * p1 + (T(1.0 / 6.0) * B) * p2;
        return firstRow + secondRow + thirdRow + fourthRow;
    }

    template<class D, typename T = Vec<D>>
    inline T MitchellNetravaliV(const D df, T d, T p0, T p1, T p2, T p3) {
        const T C = Set(df, 1.0 / 3.0);
        const T B = Set(df, 1.0 / 3.0);
        return CubicSplineGeneric<D, T>(df, C, B, d, p0, p1, p2, p3);
    }

    template<class D, typename T = Vec<D>>
    T CubicBSplineV(const D df, T d, T p0, T p1, T p2, T p3) {
        const T C = Set(df, 0.0);
        const T B = Set(df, 1.0);
        return CubicSplineGeneric<D, T>(df, C, B, d, p0, p1, p2, p3);
    }

    template<class D, typename T = Vec<D>>
    T SimpleCubicV(const D df, T t, T A, T B, T C, T D1) {
        T duplet = Mul(t, t);
        T triplet = Mul(duplet, t);
        T a = Add(Sub(Add(Mul(Neg(A), Set(df, 0.5)), Mul(B, Set(df, 1.5))), Mul(C, Set(df, 1.5))),
                  Mul(D1, Set(df, 0.5)));
        T b = Sub(Add(Sub(A, Mul(B, Set(df, 2.5))), Mul(Set(df, 2.0), C)), Mul(D1, Set(df, 0.5f)));
        T c = Add(Mul(Neg(A), Set(df, 0.5f)), Mul(C, Set(df, 0.5f)));
        T d = B;
        return MulAdd(Mul(a, triplet), Set(df, 3.0), MulAdd(b, duplet, MulAdd(c, t, d)));
    }

    template<typename T>
    T MitchellNetravali(T d, T p0, T p1, T p2, T p3) {
        constexpr T C = T(1.0 / 3.0);
        constexpr T B = T(1.0 / 3.0);
        T duplet = d * d;
        T triplet = duplet * d;
        T firstRow = ((T(-1 / 6.0) * B - C) * p0 + (T(-1.5) * B - C + T(2.0)) * p1 +
                      (T(1.5) * B + C - T(2.0)) * p2 + (T(1 / 6.0) * B + C) * p3) * triplet;
        T secondRow = ((T(0.5) * B + 2 * C) * p0 + (T(2.0) * B + C - T(3.0)) * p1 +
                       (T(-2.5) * B - T(2.0) * C + T(3.0)) * p2 - C * p3) * duplet;
        T thirdRow = ((T(-0.5) * B - C) * p0 + (T(0.5) * B + C) * p2) * d;
        T fourthRow =
                (T(1.0 / 6.0) * B) * p0 + (T(-1.0 / 3.0) * B + T(1)) * p1 + (T(1.0 / 6.0) * B) * p2;
        return firstRow + secondRow + thirdRow + fourthRow;
    }

    template<class D, typename T = Vec<D>>
    inline T fastSinV(const D d, T x) {
        const T A = Set(d, 4.0 / (M_PI * M_PI));
        const T P = Set(d, 0.1952403377008734);
        const T Q = Set(d, 0.01915214119105392);

        T y = Mul(Mul(A, x), Sub(Set(d, M_PI), x));

        const T ones = Set(d, 1);
        T a1 = Sub(Sub(ones, P), Q);
        T a2 = MulAdd(y, Q, P);
        return MulAdd(a2, y, a1);
    }

    template<class D, typename T = Vec<D>>
    inline T sincV(const D d, T x) {
        const T ones = Set(d, 1);
        const T zeros = Zero(d);
        auto maskEqualToZero = x == zeros;
        T sine = fastSinV(d, x);
        x = IfThenElse(maskEqualToZero, ones, x);
        T result = Div(sine, x);
        result = IfThenElse(maskEqualToZero, ones, result);
        return result;
    }

    template<typename T>
    inline T sinc(T x) {
        if (x == 0.0) {
            return T(1.0);
        } else {
            return fastSin2(x) / x;
        }
    }

    template<typename T>
    inline T LanczosWindow(T x, const T a) {
        if (abs(x) < a) {
            return sinc(T(M_PI) * x) * sinc(T(M_PI) * x / a);
        }
        return T(0.0);
    }

    template<typename T>
    inline T fastCos(T x) {
        constexpr T C0 = 0.99940307;
        constexpr T C1 = -0.49558072;
        constexpr T C2 = 0.03679168;
        constexpr T C3 = -0.00434102;

        while (x < -2 * M_PI) {
            x += 2.0 * M_PI;
        }
        while (x > 2 * M_PI) {
            x -= 2.0 * M_PI;
        }

        // Calculate cos(x) using Chebyshev polynomial approximation
        T x2 = x * x;
        T result = C0 + x2 * (C1 + x2 * (C2 + x2 * C3));
        return result;
    }

    template<typename T>
    inline T HannWindow(T x, const T length) {
        const float size = length * 2 - 1;
        if (abs(x) <= size) {
            return 0.5f * (1 - fastCos(T(2) * T(M_PI) * size / length));
        }
        return T(0);
    }

    template<typename T>
    inline T CatmullRom(T x, T p0, T p1, T p2, T p3) {
        if (x < 0 || x > 1) {
            return T(0);
        }
        T s1 = x * (-p0 + 3 * p1 - 3 * p2 + p3);
        T s2 = (T(2.0) * p0 - T(5.0) * p1 + 4 * p2 - p3);
        T s3 = (s1 + s2) * x;
        T s4 = (-p0 + p2);
        T s5 = (s3 + s4) * x * T(0.5);

        return s5 + p1;
    }

    template<class D, typename T = Vec<D>>
    inline T CatmullRomV(const D df, T d, T p0, T p1, T p2, T p3) {
        const T ones = Set(df, 1.0);
        const T zeros = Zero(df);
        auto maskLessThanZero = d < zeros;
        auto maskGreaterThanZero = d > ones;
        const T threes = Set(df, 3.0);

        T s1 = Add(MulSub(threes, p1, p0), NegMulAdd(threes, p2, p3));
        T s2 = Add(MulSub(Set(df, 2.0), p0, p3), MulSub(Set(df, 4.0), p2, Mul(Set(df, 5.0), p1)));
        T s3 = Mul(Add(s1, s2), d);
        T s4 = Sub(p2, p0);
        T s5 = Mul(Mul(Add(s3, s4), Set(df, 0.5)), d);

        T result = Add(s5, p1);
        result = IfThenElse(maskLessThanZero, zeros, result);
        result = IfThenElse(maskGreaterThanZero, zeros, result);
        return result;
    }

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

                    float kWeightSum = 0;
                    VF4 color = Set(dfx4, 0);

                    for (int j = -a + 1; j <= a; j++) {
                        int yj = (int) ky1 + j;
                        float dy = float(srcY) - (float(ky1) + (float) j);
                        float yWeight = sampler(dy, lanczosFA);
                        auto row = reinterpret_cast<const uint16_t *>(src8 +
                                                                      clamp(yj, 0,
                                                                            inputHeight - 1) *
                                                                      srcStride);
                        for (int i = -a + 1; i <= a; i++) {
                            int xi = (int) kx1 + i;
                            float dx = float(srcX) - (float(kx1) + (float) i);
                            float weight = sampler(dx, lanczosFA) * yWeight;

                            kWeightSum += weight;

                            int sizeXPos = clamp(xi, 0, mMaxWidth) * components;
                            VF16x4 r1 = LoadU(df16x4,
                                              reinterpret_cast<const float16_t *>(&row[sizeXPos]));
                            VF4 fr1 = PromoteTo(dfx4, r1);
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
                        float yWeight = sampler(dy, lanczosFA);
                        for (int i = -a + 1; i <= a; i++) {
                            int xi = (int) kx1 + i;
                            float dx = float(srcX) - (float(kx1) + (float) i);
                            float weight = sampler(dx, lanczosFA) * yWeight;
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
                for (int c = 0; c < components; ++c) {
                    dst16[x * components + c] = row[x1 * components + c];
                }
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
                    for (int j = -a + 1; j <= a; j++) {
                        int yj = (int) ky1 + j;
                        float dy = float(srcY) - (float(ky1) + (float) j);
                        float yWeight = sampler(dy, (float) a);
                        auto srcRow = reinterpret_cast<const uint8_t *>(src8 +
                                                                        clamp(yj, 0, mMaxHeight) *
                                                                        srcStride);
                        for (int i = -a + 1; i <= a; i++) {
                            int xi = (int) kx1 + i;
                            float dx = float(srcX) - (float(kx1) + (float) i);
                            float weight = sampler(dx, (float) a) * yWeight;
                            kWeightSum += weight;

                            int sizeXPos = clamp(xi, 0, mMaxWidth) * components;
                            VU8x4 r1 = LoadU(du8x4,
                                             reinterpret_cast<const uint8_t *>(&srcRow[sizeXPos]));
                            VF4 fr1 = ConvertTo(dfx4, PromoteTo(dux4, r1));
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
                        float yWeight = sampler(dy, (float) lanczosFA);
                        for (int i = -a + 1; i <= a; i++) {
                            int xi = (int) kx1 + i;
                            float dx = float(srcX) - (float(kx1) + (float) i);
                            float weight = sampler(dx, (float) lanczosFA) * yWeight;
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
                auto row = reinterpret_cast<const uint8_t *>(src8 + y1 * srcStride);
                for (int c = 0; c < components; ++c) {
                    dst[x * components + c] = row[x1 * components + c];
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