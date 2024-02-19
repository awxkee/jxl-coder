/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 15/09/2023
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

#include "Rgb565.h"
#include "HalfFloats.h"
#include <thread>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "Rgb565.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "imagebit/attenuate-inl.h"
#include "algo/math-inl.h"

using namespace std;

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

    using namespace hwy::HWY_NAMESPACE;
    using hwy::float16_t;
    using hwy::float32_t;

    void
    Rgba8To565HWYRow(const uint8_t *source, uint16_t *destination, int width,
                     const bool attenuateAlpha) {
        const FixedTag<uint16_t, 8> du16;
        const FixedTag<uint8_t, 16> du8x16;
        using VU16 = Vec<decltype(du16)>;
        using VU8x16 = Vec<decltype(du8x16)>;

        int x = 0;
        int pixels = 16;

        auto src = reinterpret_cast<const uint8_t *>(source);
        auto dst = reinterpret_cast<uint16_t *>(destination);
        for (; x + pixels < width; x += pixels) {
            VU8x16 ru8Row;
            VU8x16 gu8Row;
            VU8x16 bu8Row;
            VU8x16 au8Row;

            LoadInterleaved4(du8x16, reinterpret_cast<const uint8_t *>(src),
                             ru8Row, gu8Row, bu8Row, au8Row);

            if (attenuateAlpha) {
                ru8Row = AttenuateVec(du8x16, ru8Row, au8Row);
                gu8Row = AttenuateVec(du8x16, gu8Row, au8Row);
                bu8Row = AttenuateVec(du8x16, bu8Row, au8Row);
            }

            auto rdu16Vec = ShiftLeft<11>(ShiftRight<3>(PromoteLowerTo(du16, ru8Row)));
            auto gdu16Vec = ShiftLeft<5>(ShiftRight<2>(PromoteLowerTo(du16, gu8Row)));
            auto bdu16Vec = ShiftRight<3>(PromoteLowerTo(du16, bu8Row));

            auto result = Or(Or(rdu16Vec, gdu16Vec), bdu16Vec);
            StoreU(result, du16, dst);

            rdu16Vec = ShiftLeft<11>(ShiftRight<3>(PromoteUpperTo(du16, ru8Row)));
            gdu16Vec = ShiftLeft<5>(ShiftRight<2>(PromoteUpperTo(du16, gu8Row)));
            bdu16Vec = ShiftRight<3>(PromoteUpperTo(du16, bu8Row));

            result = Or(Or(rdu16Vec, gdu16Vec), bdu16Vec);
            Store(result, du16, dst + 8);

            src += 4 * pixels;
            dst += pixels;
        }

        for (; x < width; ++x) {
            uint8_t alpha = src[3];
            uint8_t r = src[0];
            uint8_t g = src[1];
            uint8_t b = src[2];

            if (attenuateAlpha) {
                r = (r * alpha + 127) / 255;
                g = (g * alpha + 127) / 255;
                b = (b * alpha + 127) / 255;
            }

            uint16_t red565 = (r >> 3) << 11;
            uint16_t green565 = (g >> 2) << 5;
            uint16_t blue565 = b >> 3;

            auto result = static_cast<uint16_t>(red565 | green565 | blue565);
            dst[0] = result;

            src += 4;
            dst += 1;
        }
    }

    void
    Rgb565ToF16HWYRow(const uint16_t *source, uint16_t *destination, int width,
                      const int *permuteMap) {
        const FixedTag<uint16_t, 8> du16;
        const FixedTag<hwy::float16_t, 8> df16;
        const FixedTag<hwy::float16_t, 4> df16x4;
        const FixedTag<uint32_t, 4> du32;
        const FixedTag<float, 4> df;
        using VU16 = Vec<decltype(du16)>;
        using VF16 = Vec<decltype(df16)>;
        using VF = Vec<decltype(df)>;

        int x = 0;
        int pixels = 8;

        auto src = reinterpret_cast<const uint16_t *>(source);
        auto dst = reinterpret_cast<uint16_t *>(destination);

        const int permute0Value = permuteMap[0];
        const int permute1Value = permuteMap[1];
        const int permute2Value = permuteMap[2];
        const int permute3Value = permuteMap[3];

        const VU16 redBytes = Set(du16, 0b1111100000000000);
        const VU16 greenBytes = Set(du16, 0b11111100000);
        const VU16 blueBytes = Set(du16, 0b11111);
        const VF16 bgPixel = Set(df16, hwy::F16FromF32(1.0f));
        const VF max8BitColors = ApproximateReciprocal(Set(df, 255));

        for (; x + pixels < width; x += pixels) {
            VU16 row = LoadU(du16, src);
            auto rdu16Vec = ShiftRight<8>(And(row, redBytes));
            auto gdu16Vec = ShiftRight<3>(And(row, greenBytes));
            auto bdu16Vec = ShiftLeft<3>(And(row, blueBytes));

            auto frdu16Vec = Combine(df16,
                                     DemoteTo(df16x4,
                                              Mul(ConvertTo(df, PromoteUpperTo(du32, rdu16Vec)),
                                                  max8BitColors)),
                                     DemoteTo(df16x4,
                                              Mul(ConvertTo(df, PromoteLowerTo(du32, rdu16Vec)),
                                                  max8BitColors)));
            auto fgdu16Vec = Combine(df16,
                                     DemoteTo(df16x4,
                                              Mul(ConvertTo(df, PromoteUpperTo(du32, gdu16Vec)),
                                                  max8BitColors)),
                                     DemoteTo(df16x4,
                                              Mul(ConvertTo(df, PromoteLowerTo(du32, gdu16Vec)),
                                                  max8BitColors)));
            auto fbdu16Vec = Combine(df16,
                                     DemoteTo(df16x4,
                                              Mul(ConvertTo(df, PromoteUpperTo(du32, bdu16Vec)),
                                                  max8BitColors)),
                                     DemoteTo(df16x4,
                                              Mul(ConvertTo(df, PromoteLowerTo(du32, bdu16Vec)),
                                                  max8BitColors)));

            VF16 pixelsStore[4] = {bgPixel, frdu16Vec, fgdu16Vec, fbdu16Vec};
            VF16 AV = pixelsStore[permute0Value];
            VF16 RV = pixelsStore[permute1Value];
            VF16 GV = pixelsStore[permute2Value];
            VF16 BV = pixelsStore[permute3Value];

            StoreInterleaved4(BitCast(du16, AV), BitCast(du16, RV), BitCast(du16, GV),
                              BitCast(du16, BV), du16, reinterpret_cast<uint16_t *>(dst));

            src += pixels;
            dst += pixels * 4;
        }

        constexpr float approxMaxColors = 1 / 255.0f;

        for (; x < width; ++x) {
            uint16_t color565 = src[0];

            uint16_t red565 = (color565 & 0b1111100000000000) >> 8;
            uint16_t green565 = (color565 & 0b11111100000) >> 3;
            uint16_t blue565 = (color565 & 0b11111) << 3;

            uint16_t clr[4] = {float_to_half(1.0f),
                               float_to_half(static_cast<float>(red565) * approxMaxColors),
                               float_to_half(static_cast<float>(green565) * approxMaxColors),
                               float_to_half(static_cast<float>(blue565) * approxMaxColors)};

            dst[0] = clr[permute0Value];
            dst[1] = clr[permute1Value];
            dst[2] = clr[permute2Value];
            dst[3] = clr[permute3Value];

            src += 1;
            dst += 4;
        }
    }

    void
    Rgb565ToU8HWYRow(const uint16_t *source, uint8_t *destination, int width,
                     const int *permuteMap, const uint8_t bgColor) {
        const FixedTag<uint16_t, 8> du16;
        const FixedTag<uint8_t, 8> du8x8;
        using VU16 = Vec<decltype(du16)>;
        using VU8x8 = Vec<decltype(du8x8)>;

        int x = 0;
        int pixels = 8;

        auto src = reinterpret_cast<const uint16_t *>(source);
        auto dst = reinterpret_cast<uint8_t *>(destination);

        const VU16 redBytes = Set(du16, 0b1111100000000000);
        const VU16 greenBytes = Set(du16, 0b11111100000);
        const VU16 blueBytes = Set(du16, 0b11111);
        const VU8x8 bgPixel = Set(du8x8, bgColor);

        const int permute0Value = permuteMap[0];
        const int permute1Value = permuteMap[1];
        const int permute2Value = permuteMap[2];
        const int permute3Value = permuteMap[3];

        for (; x + pixels < width; x += pixels) {
            VU16 row = LoadU(du16, src);
            auto rdu16Vec = DemoteTo(du8x8, ShiftRight<8>(And(row, redBytes)));
            auto gdu16Vec = DemoteTo(du8x8, ShiftRight<3>(And(row, greenBytes)));
            auto bdu16Vec = DemoteTo(du8x8, ShiftLeft<3>(And(row, blueBytes)));

            VU8x8 pixelsStore[4] = {bgPixel, rdu16Vec, gdu16Vec, bdu16Vec};
            VU8x8 AV = pixelsStore[permute0Value];
            VU8x8 RV = pixelsStore[permute1Value];
            VU8x8 GV = pixelsStore[permute2Value];
            VU8x8 BV = pixelsStore[permute3Value];

            StoreInterleaved4(AV, RV, GV, BV, du8x8, dst);

            src += pixels;
            dst += pixels * 4;
        }

        for (; x < width; ++x) {
            uint16_t color565 = src[0];

            uint16_t red8 = (color565 & 0b1111100000000000) >> 8;
            uint16_t green8 = (color565 & 0b11111100000) >> 3;
            uint16_t blue8 = (color565 & 0b11111) << 3;

            uint8_t clr[4] = {bgColor,
                              static_cast<uint8_t >(red8),
                              static_cast<uint8_t >(green8),
                              static_cast<uint8_t >(blue8)};

            dst[0] = clr[permute0Value];
            dst[1] = clr[permute1Value];
            dst[2] = clr[permute2Value];
            dst[3] = clr[permute3Value];

            src += 1;
            dst += 4;
        }
    }

    void Rgba8To565HWY(const uint8_t *sourceData, const int srcStride,
                       uint16_t *dst, const int dstStride, const int width,
                       const int height, const int bitDepth, const bool attenuateAlpha) {

        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

#pragma omp parallel for num_threads(3) schedule(dynamic)
        for (int y = 0; y < height; ++y) {
            Rgba8To565HWYRow(
                    reinterpret_cast<const uint8_t *>(mSrc + srcStride * y),
                    reinterpret_cast<uint16_t *>(mDst + dstStride * y),
                    width, attenuateAlpha);
        }
    }

    inline Vec<FixedTag<uint8_t, 8>>
    ConvertF16ToU16Row(Vec<FixedTag<uint16_t, 8>> v, float maxColors) {
        FixedTag<float16_t, 4> df16;
        FixedTag<uint16_t, 4> dfu416;
        FixedTag<uint8_t, 8> du8;
        Rebind<float, decltype(df16)> rf32;
        Rebind<int32_t, decltype(rf32)> ri32;
        Rebind<uint8_t, decltype(rf32)> ru8;

        using VU8 = Vec<decltype(du8)>;

        auto minColors = Zero(rf32);
        auto vMaxColors = Set(rf32, (int) maxColors);

        auto lower = DemoteTo(ru8, ConvertTo(ri32,
                                             ClampRound(rf32, Mul(
                                                     PromoteTo(rf32, BitCast(df16, LowerHalf(v))),
                                                     vMaxColors), minColors, vMaxColors)
        ));
        auto upper = DemoteTo(ru8, ConvertTo(ri32,
                                             ClampRound(rf32, Mul(PromoteTo(rf32,
                                                                            BitCast(df16,
                                                                                    UpperHalf(
                                                                                            dfu416,
                                                                                            v))),
                                                                  vMaxColors), minColors,
                                                        vMaxColors)
        ));
        return Combine(du8, upper, lower);
    }

    void
    RGBAF16To565RowHWY(const uint16_t *source, uint16_t *destination, int width,
                       float maxColors) {
        const FixedTag<uint16_t, 8> du16;
        const FixedTag<uint8_t, 8> du8;
        using VU16 = Vec<decltype(du16)>;
        using VU8 = Vec<decltype(du8)>;

        Rebind<uint16_t, FixedTag<uint8_t, 8>> rdu16;

        int x = 0;
        int pixels = 8;

        auto src = reinterpret_cast<const uint16_t *>(source);
        auto dst = reinterpret_cast<uint16_t *>(destination);
        for (x = 0; x + pixels < width; x += pixels) {
            VU16 ru16Row;
            VU16 gu16Row;
            VU16 bu16Row;
            VU16 au16Row;
            LoadInterleaved4(du16, reinterpret_cast<const uint16_t *>(src),
                             ru16Row, gu16Row,
                             bu16Row,
                             au16Row);

            auto r16Row = ConvertF16ToU16Row(ru16Row, maxColors);
            auto g16Row = ConvertF16ToU16Row(gu16Row, maxColors);
            auto b16Row = ConvertF16ToU16Row(bu16Row, maxColors);

            auto rdu16Vec = ShiftLeft<11>(ShiftRight<3>(PromoteTo(rdu16, r16Row)));
            auto gdu16Vec = ShiftLeft<5>(ShiftRight<2>(PromoteTo(rdu16, g16Row)));
            auto bdu16Vec = ShiftRight<3>(PromoteTo(rdu16, b16Row));

            auto result = Or(Or(rdu16Vec, gdu16Vec), bdu16Vec);
            StoreU(result, du16, dst);

            src += 4 * pixels;
            dst += pixels;
        }

        for (; x < width; ++x) {
            uint8_t r = static_cast<uint8_t >(roundf(
                    clamp(half_to_float(src[0]), 0.0f, 1.0f) * maxColors));
            uint8_t g = static_cast<uint8_t >(roundf(
                    clamp(half_to_float(src[1]), 0.0f, 1.0f) * maxColors));
            uint8_t b = static_cast<uint8_t >(roundf(
                    clamp(half_to_float(src[2]), 0.0f, 1.0f) * maxColors));

            r = clamp(r, (uint8_t) 0, (uint8_t) maxColors);
            g = clamp(g, (uint8_t) 0, (uint8_t) maxColors);
            b = clamp(b, (uint8_t) 0, (uint8_t) maxColors);

            uint16_t red565 = (r >> 3) << 11;
            uint16_t green565 = (g >> 2) << 5;
            uint16_t blue565 = b >> 3;

            uint16_t result = static_cast<uint16_t>(red565 | green565 | blue565);
            dst[0] = result;

            src += 4;
            dst += 1;
        }

    }

    inline Vec<FixedTag<uint16_t, 4>>
    ConvertF32ToU16Row(Vec<FixedTag<float, 4>> v, float maxColors) {
        FixedTag<float32_t, 4> df32;
        Rebind<int32_t, decltype(df32)> ri32;
        Rebind<uint16_t, decltype(df32)> ru8;

        auto minColors = Zero(df32);
        auto vMaxColors = Set(df32, (float) maxColors);
        auto lower = DemoteTo(ru8,
                              ConvertTo(ri32, ClampRound(df32, Mul(v, vMaxColors), minColors,
                                                         vMaxColors)));
        return lower;
    }

    void
    RGBAF32To565RowHWY(const float *source, uint16_t *destination, int width,
                       float maxColors) {
        const FixedTag<float, 4> df16;
        const FixedTag<uint16_t, 4> du16;
        const FixedTag<uint8_t, 4> du8;
        using VU16 = Vec<decltype(df16)>;
        using VU8 = Vec<decltype(du8)>;

        int x = 0;
        int pixels = 4;

        auto src = reinterpret_cast<const float *>(source);
        auto dst = reinterpret_cast<uint16_t *>(destination);
        for (x = 0; x + pixels < width; x += pixels) {
            VU16 ru16Row;
            VU16 gu16Row;
            VU16 bu16Row;
            VU16 au16Row;
            LoadInterleaved4(df16, reinterpret_cast<const float *>(src),
                             ru16Row, gu16Row,
                             bu16Row,
                             au16Row);

            auto r16Row = ConvertF32ToU16Row(ru16Row, maxColors);
            auto g16Row = ConvertF32ToU16Row(gu16Row, maxColors);
            auto b16Row = ConvertF32ToU16Row(bu16Row, maxColors);

            auto rdu16Vec = ShiftLeft<11>(ShiftRight<3>(r16Row));
            auto gdu16Vec = ShiftLeft<5>(ShiftRight<2>(g16Row));
            auto bdu16Vec = ShiftRight<3>(b16Row);

            auto result = Or(Or(rdu16Vec, gdu16Vec), bdu16Vec);
            StoreU(result, du16, dst);

            src += 4 * pixels;
            dst += pixels;
        }

        for (; x < width; ++x) {
            uint16_t red565 = ((uint16_t) (src[0] * maxColors) >> 3) << 11;
            uint16_t green565 = ((uint16_t) (src[1] * maxColors) >> 2) << 5;
            uint16_t blue565 = (uint16_t) (src[2] * maxColors) >> 3;

            auto result = static_cast<uint16_t>(red565 | green565 | blue565);
            dst[0] = result;

            src += 4;
            dst += 1;
        }

    }

    void RGBAF16To565HWY(const uint16_t *sourceData, int srcStride,
                         uint16_t *dst, int dstStride, int width,
                         int height) {
        float maxColors = powf(2, (float) 8) - 1;

        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

#pragma omp parallel for num_threads(3) schedule(dynamic)
        for (int y = 0; y < height; ++y) {
            RGBAF16To565RowHWY(
                    reinterpret_cast<const uint16_t *>(mSrc + srcStride * y),
                    reinterpret_cast<uint16_t *>(mDst + dstStride * y),
                    width, maxColors);
        }
    }

    void RGBAF32To565HWY(const float *sourceData, int srcStride,
                         uint16_t *dst, int dstStride, int width,
                         int height) {
        float maxColors = std::pow(2, (float) 8) - 1;

        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

#pragma omp parallel for num_threads(3) schedule(dynamic)
        for (int y = 0; y < height; ++y) {
            RGBAF32To565RowHWY(
                    reinterpret_cast<const float *>(mSrc + srcStride * y),
                    reinterpret_cast<uint16_t *>(mDst + dstStride * y),
                    width, maxColors);
        }
    }

    void Rgb565ToF16HWY(const uint16_t *sourceData, int srcStride,
                        uint16_t *dst, int dstStride, int width,
                        int height, const int *permuteMap) {

        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

#pragma omp parallel for num_threads(3) schedule(dynamic)
        for (int y = 0; y < height; ++y) {
            Rgb565ToF16HWYRow(
                    reinterpret_cast<const uint16_t *>(mSrc + srcStride * y),
                    reinterpret_cast<uint16_t *>(mDst + dstStride * y),
                    width, permuteMap);
        }
    }

    void Rgb565ToU8HWY(const uint16_t *sourceData, int srcStride,
                       uint8_t *dst, int dstStride, int width,
                       int height, int bitDepth, const int *permuteMap, const uint8_t bgColor) {

        auto mSrc = reinterpret_cast<const uint8_t *>(sourceData);
        auto mDst = reinterpret_cast<uint8_t *>(dst);

#pragma omp parallel for num_threads(3) schedule(dynamic)
        for (int y = 0; y < height; ++y) {
            Rgb565ToU8HWYRow(
                    reinterpret_cast<const uint16_t *>(mSrc + srcStride * y),
                    reinterpret_cast<uint8_t *>(mDst + dstStride * y),
                    width, permuteMap, bgColor);
        }
    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {

    HWY_EXPORT(Rgb565ToU8HWY);

    void Rgb565ToUnsigned8(const uint16_t *sourceData, int srcStride,
                           uint8_t *dst, int dstStride, int width,
                           int height, int bitDepth, const uint8_t bgColor) {
        int permuteMap[4] = {1, 2, 3, 0};
        HWY_DYNAMIC_DISPATCH(Rgb565ToU8HWY)(sourceData, srcStride, dst, dstStride, width, height,
                                            bitDepth, permuteMap, bgColor);
    }

    HWY_EXPORT(Rgb565ToF16HWY);

    void Rgb565ToF16(const uint16_t *sourceData, int srcStride,
                     uint16_t *dst, int dstStride, int width,
                     int height) {
        int permuteMap[4] = {1, 2, 3, 0};
        HWY_DYNAMIC_DISPATCH(Rgb565ToF16HWY)(sourceData, srcStride, dst, dstStride, width, height,
                                             permuteMap);
    }

    HWY_EXPORT(Rgba8To565HWY);
    HWY_DLLEXPORT void Rgba8To565(const uint8_t *sourceData, int srcStride,
                                  uint16_t *dst, int dstStride, int width,
                                  int height, int bitDepth, const bool attenuateAlpha) {
        HWY_DYNAMIC_DISPATCH(Rgba8To565HWY)(sourceData, srcStride, dst, dstStride, width,
                                            height, bitDepth, attenuateAlpha);
    }

    HWY_EXPORT(RGBAF16To565HWY);
    HWY_DLLEXPORT void RGBAF16To565(const uint16_t *sourceData, int srcStride,
                                    uint16_t *dst, int dstStride, int width,
                                    int height) {
        HWY_DYNAMIC_DISPATCH(RGBAF16To565HWY)(sourceData, srcStride, dst, dstStride, width,
                                              height);
    }

    HWY_EXPORT(RGBAF32To565HWY);
    HWY_DLLEXPORT void RGBAF32To565(const float *sourceData, int srcStride,
                                    uint16_t *dst, int dstStride, int width,
                                    int height) {
        HWY_DYNAMIC_DISPATCH(RGBAF32To565HWY)(sourceData, srcStride, dst, dstStride, width,
                                              height);
    }
}
#endif