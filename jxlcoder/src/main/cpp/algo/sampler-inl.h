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
using hwy::HWY_NAMESPACE::Add;
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

using hwy::HWY_NAMESPACE::NegMulAdd;
using hwy::HWY_NAMESPACE::MulAdd;
using hwy::HWY_NAMESPACE::IfThenElse;
using hwy::HWY_NAMESPACE::MulSub;
using hwy::HWY_NAMESPACE::ApproximateReciprocal;

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T
BCSplinePartOne(const D df, T x, const T B, const T C, const T tripled, const T doubled) {
    x = Abs(x);
    T mult = Set(df, 1.0f / 6.0f);
    T r1 = NegMulAdd(Set(df, 9), B, NegMulAdd(Set(df, 6.0), C, Set(df, 12)));
    T r2 = MulAdd(Set(df, 6), C, MulSub(Set(df, 12.0f), B, Set(df, 18.0f)));
    T r3 = NegMulAdd(Set(df, 2), B, Set(df, 6));
    return Mul(MulAdd(r1, tripled, MulAdd(r2, doubled, r3)), mult);
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T
BCSplinePartTwo(const D df, T x, const T B, const T C, const T tripled, const T doubled) {
    x = Abs(x);
    T mult = Set(df, 1.0f / 6.0f);
    T r1 = MulSub(Set(df, -6.0f), C, B);
    T r2 = MulAdd(Set(df, 6.0), B, Mul(Set(df, 30), C));
    T r3 = MulSub(Set(df, -12), B, Mul(Set(df, 48), C));
    T r4 = MulAdd(Set(df, 8.0), B, Mul(Set(df, 24.0f), C));
    T rr = MulAdd(r1, tripled, MulAdd(r2, doubled, MulAdd(r3, x, r4)));
    return Mul(rr, mult);
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T BCSpline(const D df, T x, const T B, const T C) {
    x = Abs(x);
    const T zeros = Zero(df);
    const T ones = Set(df, 1.0);
    const T two = Set(df, 2.0);
    const T doubled = Mul(x, x);
    const T tripled = Mul(doubled, x);
    auto setZeroMask = x > two;
    auto setP1Mask = x < ones;
    auto setP2Mask = x >= ones;
    T res = Zero(df);
    const T p1 = BCSplinePartOne(df, x, B, C, tripled, doubled);
    const T p2 = BCSplinePartTwo(df, x, B, C, tripled, doubled);
    res = IfThenElse(setP1Mask, p1, zeros);
    res = IfThenElse(setP2Mask, p2, res);
    res = IfThenElse(setZeroMask, zeros, res);
    return res;
}

#include "sampler.h"

using hwy::HWY_NAMESPACE::InsertLane;
using hwy::HWY_NAMESPACE::ExtractLane;
using hwy::HWY_NAMESPACE::LoadU;

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T MitchellNetravaliV(const D df, T d) {
    const T C = Set(df, 1.0 / 3.0);
    const T B = Set(df, 1.0 / 3.0);
    return BCSpline<D, T>(df, d, B, C);
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T CubicHermiteV(const D df, T d) {
    const T C = Set(df, 0.0);
    const T B = Set(df, 0.0);
    return BCSpline<D, T>(df, d, B, C);
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T CubicBSplineV(const D df, T d) {
    const T C = Set(df, 0.0);
    const T B = Set(df, 1.0);
    return BCSpline<D, T>(df, d, B, C);
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T BiCubicSplineV(const D df, T x, hwy::HWY_NAMESPACE::TFromD<D> a = -0.5) {
    const T aVec = Set(df, a);
    const T ones = Set(df, 1.0);
    const T two = Set(df, 2.0);
    const T three = Set(df, 3.0);
    const T four = Set(df, 4.0);
    const T five = Set(df, 5.0);
    const T eight = Set(df, 8.0);
    const T zeros = Zero(df);
    x = Abs(x);
    const auto zeroMask = x >= two;
    const auto partOneMask = x < ones;
    const T doubled = Mul(x, x);
    const T triplet = Mul(doubled, x);

    const T partOne = MulAdd(Add(two, aVec), triplet, NegMulAdd(Add(aVec, three), doubled, ones));
    const T fourA = Mul(four, aVec);
    const T eightA = Mul(eight, aVec);
    const T fiveA = Mul(five, aVec);
    const T partTwo = MulAdd(aVec, triplet,
                             NegMulAdd(fiveA, doubled,
                                       MulSub(eightA, x, fourA)));

    x = IfThenElse(partOneMask, partOne, partTwo);
    x = IfThenElse(zeroMask, zeros, x);

    return x;
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T SimpleCubicV(const D df, T x) {
    x = Abs(x);
    const T zeros = Zero(df);
    const T ones = Set(df, 1.0);
    const T two = Set(df, 2.0);
    const T doubled = Mul(x, x);
    const T tripled = Mul(doubled, x);
    auto setZeroMask = x > two;
    auto setP1Mask = x < ones;
    auto setP2Mask = x >= ones;
    const T mSix = Set(df, 6.0f);
    const T sixScale = ApproximateReciprocal(mSix);
    T res = Zero(df);
    const T p1 = Mul(MulAdd(MulSub(Set(df, 3), x, mSix), Mul(x, x), Set(df, 4.0f)), sixScale);
    const T p2 = Mul(MulAdd(MulSub(Sub(mSix, x), x, Set(df, 12.0f)), x, Set(df, 8.0f)), sixScale);
    res = IfThenElse(setP1Mask, p1, zeros);
    res = IfThenElse(setP2Mask, p2, res);
    res = IfThenElse(setZeroMask, zeros, res);
    return res;
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T sincV(const D d, T x) {
    const T ones = Set(d, 1);
    const T zeros = Zero(d);
    auto maskEqualToZero = x == zeros;
    T sine = hwy::HWY_NAMESPACE::Sin(d, x);
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
HWY_MATH_INLINE T CatmullRomV(const D df, T d) {
    const T C = Set(df, 0.0);
    const T B = Set(df, 0.5);
    return BCSpline<D, T>(df, d, B, C);
}

using hwy::HWY_NAMESPACE::Lerp;

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T Blerp(const D df, T c00, T c10, T c01, T c11, T tx, T ty) {
    return Lerp(df, Lerp(df, c00, c10, tx), Lerp(df, c01, c11, tx), ty);
}

template<class D, typename T = Vec<D>>
HWY_MATH_INLINE T HannWindow(const D df, const T n, const float length) {
    const float size = length * 2;
    const T sizeV = Set(df, size);
    const T lengthV = Set(df, length);
    auto mask = Abs(n) > Set(df, length);
    const T piMulSize = Set(df, M_PI / size);
    T res = hwy::HWY_NAMESPACE::Cos(df, Mul(piMulSize, n));
    res = Mul(Mul(res, res), ApproximateReciprocal(sizeV));
    res = IfThenZeroElse(mask, res);
    return res;
}


#endif