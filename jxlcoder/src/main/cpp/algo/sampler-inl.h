/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 11/01/2024
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

#if defined(HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#undef HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#else
#define HIGHWAY_HWY_EXAMPLES_SKELETON_INL_H_
#endif

#include "hwy/highway.h"
#include "math-inl.h"

using hwy::HWY_NAMESPACE::Set;
using hwy::HWY_NAMESPACE::FixedTag;
using hwy::HWY_NAMESPACE::Vec;
using hwy::HWY_NAMESPACE::Abs;
using hwy::HWY_NAMESPACE::Mul;
using hwy::HWY_NAMESPACE::Div;
using hwy::HWY_NAMESPACE::Max;
using hwy::HWY_NAMESPACE::Min;
using hwy::HWY_NAMESPACE::Zero;
using hwy::HWY_NAMESPACE::BitCast;
using hwy::HWY_NAMESPACE::ConvertTo;
using hwy::HWY_NAMESPACE::PromoteTo;
using hwy::HWY_NAMESPACE::DemoteTo;
using hwy::HWY_NAMESPACE::Combine;
using hwy::HWY_NAMESPACE::Rebind;
using hwy::HWY_NAMESPACE::Sub;
using hwy::HWY_NAMESPACE::LowerHalf;
using hwy::HWY_NAMESPACE::UpperHalf;
using hwy::HWY_NAMESPACE::LoadInterleaved4;
using hwy::HWY_NAMESPACE::StoreInterleaved4;
using hwy::HWY_NAMESPACE::IfThenZeroElse;
using hwy::float16_t;
using hwy::float32_t;

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T
CubicSplineGeneric(const D df, T C, T B, T d, T p0, T p1, T p2, T p3) {
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
HWY_MATH_INLINE T CubicHermiteV(const D df, T d, T p0, T p1, T p2, T p3) {
    const T C = Set(df, 0.0);
    const T B = Set(df, 0.0);
    return CubicSplineGeneric<D, T>(df, C, B, d, p0, p1, p2, p3);
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T MitchellNetravaliV(const D df, T d, T p0, T p1, T p2, T p3) {
    const T C = Set(df, 1.0 / 3.0);
    const T B = Set(df, 1.0 / 3.0);
    return CubicSplineGeneric<D, T>(df, C, B, d, p0, p1, p2, p3);
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T CubicBSplineV(const D df, T d, T p0, T p1, T p2, T p3) {
    const T C = Set(df, 0.0);
    const T B = Set(df, 1.0);
    return CubicSplineGeneric<D, T>(df, C, B, d, p0, p1, p2, p3);
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T SimpleCubicV(const D df, T t, T A, T B, T C, T D1) {
    T duplet = Mul(t, t);
    T triplet = Mul(duplet, t);
    T a = Add(Sub(Add(Mul(Neg(A), Set(df, 0.5)), Mul(B, Set(df, 1.5))), Mul(C, Set(df, 1.5))),
              Mul(D1, Set(df, 0.5)));
    T b = Sub(Add(Sub(A, Mul(B, Set(df, 2.5))), Mul(Set(df, 2.0), C)), Mul(D1, Set(df, 0.5f)));
    T c = Add(Mul(Neg(A), Set(df, 0.5f)), Mul(C, Set(df, 0.5f)));
    T d = B;
    return MulAdd(Mul(a, triplet), Set(df, 3.0), MulAdd(b, duplet, MulAdd(c, t, d)));
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T sincV(const D d, T x) {
    const T ones = Set(d, 1);
    const T zeros = Zero(d);
    auto maskEqualToZero = x == zeros;
    T sine = coder::HWY_NAMESPACE::FastSinf(d, x);
    x = IfThenElse(maskEqualToZero, ones, x);
    T result = Div(sine, x);
    result = IfThenElse(maskEqualToZero, ones, result);
    return result;
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T LanczosWindowHWY(const D df, T x, const T a) {
    auto mask = Abs(x) >= a;
    T v = Mul(Set(df, M_PI), x);
    T r = Mul(sincV(df, v), sincV(df, Div(v, a)));
    return IfThenZeroElse(mask, r);
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T CatmullRomV(const D df, T d, T p0, T p1, T p2, T p3) {
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

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T HannWindow(const D df, const T n, const float length) {
    const float size = length * 2;
    const T lengthV = Set(df, length);
    auto mask = Abs(n) > lengthV;
    const T piMulSize = Set(df, M_PI / length);
    T res = coder::HWY_NAMESPACE::FastSinf(df, Mul(piMulSize, n));
    res = Mul(res, res);
    res = IfThenZeroElse(mask, res);
    return res;
}


#endif