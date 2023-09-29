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
#include "ThreadPool.hpp"

using namespace half_float;
using namespace std;

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

template<typename T>
inline T DemoteHalfTo(half t, float maxColors) {
    return (T) clamp(((float) t * (float) maxColors), 0.0f, (float) maxColors);
}

template<typename T>
inline T CubicBSpline(T t) {
    T absX = abs(t);
    if (absX <= 1.0) {
        return T(2.0 / 3.0) - (absX * absX) + (T(0.5) * absX * absX * absX);
    } else if (absX <= 2.0) {
        return ((T(2.0) - absX) * (T(2.0) - absX) * (T(2.0) - absX)) / T(6.0);
    } else {
        return T(0.0);
    }
}

template<typename T>
inline T FilterMitchell(T t) {
    T x = abs(t);

    if (x < 1.0f)
        return (T(16) + x * x * (T(21) * x - T(36))) / T(18);
    else if (x < 2.0f)
        return (T(32) + x * (T(-60) + x * (T(36) - T(7) * x))) / T(18);

    return T(0.0f);
}

template<typename T>
inline T sinc(T x) {
    if (x == 0.0) {
        return T(1.0);
    } else {
        return sin(x) / x;
    }
}

template<typename T>
inline T lanczosWindow(T x, T a) {
    if (abs(x) < a) {
        return sinc(T(M_PI) * x) * sinc(T(M_PI) * x / a);
    }
    return T(0.0);
    //    if (x == 0.0) {
    //        return T(1.0);
    //    }
    //    if (abs(x) < a) {
    //        return a * sin(T(M_PI) * x) * sin(T(M_PI) * x / a) / (T(M_PI) * T(M_PI) * x * x);
    //    }
    //    return T(0.0);
}

template<typename T>
inline T CatmullRom1D(T x) {
    x = (T) abs(x);

    if (x < 1.0f)
        return T(1) - x * x * (T(2.5f) - T(1.5f) * x);
    else if (x < 2.0f)
        return T(2) - x * (T(4) + x * (T(0.5f) * x - T(2.5f)));

    return T(0.0f);
}

template<typename T>
inline T CatmullRom(T x, T p0, T p1, T p2, T p3) {
    x = abs(x);

    if (x < T(1.0))
        return T(0.5) * (((T(2.0) * p1) +
                          (-p0 + p2) * x +
                          (T(2.0) * p0 - T(5.0) * p1 + T(4.0) * p2 - p3) * x * x +
                          (-p0 + T(3.0) * p1 - T(3.0) * p2 + p3) * x * x * x));

    return T(0.0);
}

void scaleRowF16(const uint8_t *src8, int srcStride, int dstStride, int inputWidth, int inputHeight,
                 uint16_t *output, int outputWidth, float xScale, const XSampler &option,
                 float yScale, int y, int components) {
    auto dst8 = reinterpret_cast<uint8_t *>(output) + y * dstStride;
    auto dst16 = reinterpret_cast<uint16_t *>(dst8);
    for (int x = 0; x < outputWidth; ++x) {
        float srcX = (float) x * xScale;
        float srcY = (float) y * yScale;

        // Calculate the integer and fractional parts
        int x1 = static_cast<int>(srcX);
        int y1 = static_cast<int>(srcY);

        if (option == bilinear) {
            int x2 = min(x1 + 1, inputWidth - 1);
            int y2 = min(y1 + 1, inputHeight - 1);

            half dx((float) x2 - (float) x1);
            half dy((float) y2 - (float) y1);

            half invertDx = half(1.0f) - dx;
            half invertDy = half(1.0f) - dy;

            for (int c = 0; c < components; ++c) {
                half c1 = castU16(reinterpret_cast<const uint16_t *>(src8 + y1 * srcStride)[
                                          x1 * components + c]) * invertDx * invertDy;
                half c2 = castU16(reinterpret_cast<const uint16_t *>(src8 + y1 * srcStride)[
                                          x2 * components + c]) * dx * invertDy;
                half c3 = castU16(reinterpret_cast<const uint16_t *>(src8 + y2 * srcStride)[
                                          x1 * components + c]) * invertDx * dy;
                half c4 = castU16(reinterpret_cast<const uint16_t *>(src8 + y2 * srcStride)[
                                          x2 * components + c]) * dx * dy;

                half result = c1 + c2 + c3 + c4;
                dst16[x * components + c] = result.data_;
            }
        } else if (option == cubic) {

            half rgb[components];

            for (int j = -1; j <= 2; j++) {
                for (int i = -1; i <= 2; i++) {
                    int xi = x1 + i;
                    int yj = y1 + j;

                    half weight = CubicBSpline(half(srcX - (float) xi)) *
                                  CubicBSpline(half(srcY - (float) yj));

                    for (int c = 0; c < components; ++c) {
                        half clrf = castU16(reinterpret_cast<const uint16_t *>(src8 + clamp(yj, 0,
                                                                                            inputHeight -
                                                                                            1) *
                                                                                      srcStride)[
                                                    clamp(x, 0, inputWidth - 1) * components + c]);
                        half clr = clrf * weight;
                        rgb[c] += clr;
                    }
                }
            }

            for (int c = 0; c < components; ++c) {
                dst16[x * components + c] = rgb[c].data_;
            }
        } else if (option == mitchell) {
            half rgb[components];

            for (int j = -1; j <= 2; j++) {
                for (int i = -1; i <= 2; i++) {
                    int xi = x1 + i;
                    int yj = y1 + j;

                    half weight = FilterMitchell(half(srcX - (float) xi)) *
                                  FilterMitchell(half(srcY - (float) yj));

                    for (int c = 0; c < components; ++c) {
                        half clrf = castU16(reinterpret_cast<const uint16_t *>(src8 + clamp(yj, 0,
                                                                                            inputHeight -
                                                                                            1) *
                                                                                      srcStride)[
                                                    clamp(xi, 0, inputWidth - 1) * components + c]);
                        half clr = clrf * weight;
                        rgb[c] += clr;
                    }
                }
            }

            for (int c = 0; c < components; ++c) {
                dst16[x * components + c] = rgb[c].data_;
            }
        } else if (option == catmullRom) {
            float kx1 = floor(srcX);
            float ky1 = floor(srcY);

            int xi = (int) kx1;
            int yj = (int) ky1;

            for (int c = 0; c < components; ++c) {
                half weight = CatmullRom(half(srcX - (float) xi),
                                         castU16(reinterpret_cast<const uint16_t *>(src8 +
                                                                                    clamp(yj, 0,
                                                                                          inputHeight -
                                                                                          1) *
                                                                                    srcStride)[
                                                         clamp(xi, 0, inputWidth - 1) * components +
                                                         c]),
                                         castU16(reinterpret_cast<const uint16_t *>(src8 +
                                                                                    clamp(yj + 1, 0,
                                                                                          inputHeight -
                                                                                          1) *
                                                                                    srcStride)[
                                                         clamp(xi + 1, 0, inputWidth - 1) *
                                                         components + c]),
                                         castU16(reinterpret_cast<const uint16_t *>(src8 +
                                                                                    clamp(yj, 0,
                                                                                          inputHeight -
                                                                                          1) *
                                                                                    srcStride)[
                                                         clamp(xi + 1, 0, inputWidth - 1) *
                                                         components + c]),
                                         castU16(reinterpret_cast<const uint16_t *>(src8 +
                                                                                    clamp(yj + 1, 0,
                                                                                          inputHeight -
                                                                                          1) *
                                                                                    srcStride)[
                                                         clamp(xi + 1, 0, inputWidth - 1) *
                                                         components + c]));
                half clr = weight;
                dst16[x * components + c] += clr.data_;
            }
        } else if (option == lanczos) {
            half rgb[components];

            int a = 3;
            half lanczosFA = half(3.0f);

            float kx1 = floor(srcX);
            float ky1 = floor(srcY);

            half weightSum(0.0f);

            for (int j = -a + 1; j <= a; j++) {
                for (int i = -a + 1; i <= a; i++) {
                    int xi = (int) kx1 + i;
                    int yj = (int) ky1 + j;
                    half dx = half(srcX) - (half(kx1) + half((float) i));
                    half dy = half(srcY) - (half(ky1) + half((float) j));
                    half weight = lanczosWindow(dx, lanczosFA) *
                                  lanczosWindow(dy, lanczosFA);
                    weightSum += weight;
                    for (int c = 0; c < components; ++c) {
                        half clrf = castU16(reinterpret_cast<const uint16_t *>(src8 + clamp(yj, 0,
                                                                                            inputHeight -
                                                                                            1) *
                                                                                      srcStride)[
                                                    clamp(xi, 0, inputWidth - 1) * components + c]);
                        half clr = half(clrf * weight);
                        rgb[c] += clr;
                    }
                }
            }

            for (int c = 0; c < components; ++c) {
                if (weightSum == 0) {
                    dst16[x * components + c] = rgb[c].data_;
                } else {
                    dst16[x * components + c] = (rgb[c] / weightSum).data_;
                }
            }
        } else {
            for (int c = 0; c < components; ++c) {
                dst16[x * components + c] = reinterpret_cast<const uint16_t *>(src8 +
                                                                               y1 * srcStride)[
                        x1 * components + c];
            }
        }
    }
}

void scaleImageFloat16(const uint16_t *input,
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

    if (inputHeight * inputWidth > 800 * 800) {
        ThreadPool pool;
        std::vector<std::future<void>> results;

        for (int y = 0; y < outputHeight; y++) {
            auto r = pool.enqueue(scaleRowF16, src8, srcStride, dstStride, inputWidth, inputHeight,
                                  output, outputWidth,
                                  xScale, option, yScale, y, components);
            results.push_back(std::move(r));
        }

        for (auto &result: results) {
            result.wait();
        }
    } else {
        for (int y = 0; y < outputHeight; y++) {
            scaleRowF16(src8, srcStride, dstStride, inputWidth, inputHeight,
                        output, outputWidth,
                        xScale, option, yScale, y, components);
        }
    }
}

void
ScaleRowU8(const uint8_t *src8, int srcStride, int inputWidth, int inputHeight, uint8_t *output,
           int dstStride,
           int outputWidth, int components, XSampler option, float xScale, float yScale,
           float maxColors, int y) {
    auto dst8 = reinterpret_cast<uint8_t *>(output + y * dstStride);
    auto dst = reinterpret_cast<uint8_t *>(dst8);
    for (int x = 0; x < outputWidth; ++x) {
        float srcX = (float) x * xScale;
        float srcY = (float) y * yScale;

        // Calculate the integer and fractional parts
        int x1 = static_cast<int>(srcX);
        int y1 = static_cast<int>(srcY);

        if (option == bilinear) {
            int x2 = std::min(x1 + 1, inputWidth - 1);
            int y2 = std::min(y1 + 1, inputHeight - 1);

            half dx((float) x2 - (float) x1);
            half dy((float) y2 - (float) y1);

            half invertDx = half(1.0f) - dx;
            half invertDy = half(1.0f) - dy;

            for (int c = 0; c < components; ++c) {
                half c1 = PromoteToHalf(
                        reinterpret_cast<const uint8_t *>(src8 + y1 * srcStride)[x1 * components +
                                                                                 c], maxColors) *
                          invertDx * invertDy;
                half c2 = PromoteToHalf(
                        reinterpret_cast<const uint8_t *>(src8 + y1 * srcStride)[x2 * components +
                                                                                 c], maxColors) *
                          dx * invertDy;
                half c3 = PromoteToHalf(
                        reinterpret_cast<const uint8_t *>(src8 + y2 * srcStride)[x1 * components +
                                                                                 c], maxColors) *
                          invertDx * dy;
                half c4 = PromoteToHalf(
                        reinterpret_cast<const uint8_t *>(src8 + y2 * srcStride)[x2 * components +
                                                                                 c], maxColors) *
                          dx * dy;

                half result = (c1 + c2 + c3 + c4);
                float f = result * maxColors;
                f = clamp(f, 0.0f, maxColors);
                dst[x * components + c] = static_cast<uint8_t>(f);

            }
        } else if (option == cubic) {
            half rgb[components];

            for (int j = -1; j <= 2; j++) {
                for (int i = -1; i <= 2; i++) {
                    int xi = x1 + i;
                    int yj = y1 + j;

                    half weight = CubicBSpline(half(srcX - (float) xi)) *
                                  CubicBSpline(half(srcY - (float) yj));

                    for (int c = 0; c < components; ++c) {
                        uint8_t p = reinterpret_cast<const uint8_t *>(src8 + clamp(yj, 0,
                                                                                   inputHeight -
                                                                                   1) * srcStride)[
                                clamp(xi, 0, inputWidth - 1) * components + c];
                        half clrf((float) p / maxColors);
                        half clr = clrf * weight;
                        rgb[c] += clr;
                    }
                }
            }

            for (int c = 0; c < components; ++c) {
                float cc = rgb[c];
                float f = cc * maxColors;
                f = std::clamp(f, 0.0f, maxColors);
                dst[x * components + c] = static_cast<uint16_t>(f);
            }
        } else if (option == mitchell) {
            half rgb[components];

            for (int j = -1; j <= 2; j++) {
                for (int i = -1; i <= 2; i++) {
                    int xi = x1 + i;
                    int yj = y1 + j;

                    half weight = FilterMitchell(half(srcX - (float) xi)) *
                                  FilterMitchell(half(srcY - (float) yj));

                    for (int c = 0; c < components; ++c) {
                        uint8_t p = reinterpret_cast<const uint8_t *>(src8 + clamp(yj, 0,
                                                                                   inputHeight -
                                                                                   1) * srcStride)[
                                clamp(xi, 0, inputWidth - 1) * components + c];
                        half clrf((float) p / maxColors);
                        half clr = clrf * weight;
                        rgb[c] += clr;
                    }
                }
            }

            for (int c = 0; c < components; ++c) {
                float cc = rgb[c];
                float f = cc * maxColors;
                f = std::clamp(f, 0.0f, maxColors);
                dst[x * components + c] = static_cast<uint16_t>(f);
            }
        } else if (option == catmullRom) {

            float kx1 = floor(srcX);
            float ky1 = floor(srcY);

            int xi = (int) kx1;
            int yj = (int) ky1;

            for (int c = 0; c < components; ++c) {
                half weight = CatmullRom(half(srcX - (float) xi),
                                         PromoteToHalf(reinterpret_cast<const uint8_t *>(src8 +
                                                                                         clamp(yj,
                                                                                               0,
                                                                                               inputHeight -
                                                                                               1) *
                                                                                         srcStride)[
                                                               clamp(xi, 0, inputWidth - 1) *
                                                               components + c], maxColors),
                                         PromoteToHalf(reinterpret_cast<const uint8_t *>(src8 +
                                                                                         clamp(yj +
                                                                                               1, 0,
                                                                                               inputHeight -
                                                                                               1) *
                                                                                         srcStride)[
                                                               clamp(xi + 1, 0, inputWidth - 1) *
                                                               components + c], maxColors),
                                         PromoteToHalf(reinterpret_cast<const uint8_t *>(src8 +
                                                                                         clamp(yj,
                                                                                               0,
                                                                                               inputHeight -
                                                                                               1) *
                                                                                         srcStride)[
                                                               clamp(xi + 1, 0, inputWidth - 1) *
                                                               components + c], maxColors),
                                         PromoteToHalf(reinterpret_cast<const uint8_t *>(src8 +
                                                                                         clamp(yj +
                                                                                               1, 0,
                                                                                               inputHeight -
                                                                                               1) *
                                                                                         srcStride)[
                                                               clamp(xi + 1, 0, inputWidth - 1) *
                                                               components + c], maxColors));
                auto clr = DemoteHalfTo<uint8_t>(weight, maxColors);
                dst[x * components + c] += clr;
            }
        } else if (option == lanczos) {
            half rgb[components];

            half lanczosFA = half(3.0f);

            int a = 3;

            float kx1 = floor(srcX);
            float ky1 = floor(srcY);

            half weightSum(0.0f);

            for (int j = -a + 1; j <= a; j++) {
                for (int i = -a + 1; i <= a; i++) {
                    int xi = (int) kx1 + i;
                    int yj = (int) ky1 + j;
                    half dx = half(srcX) - (half(kx1) + half((float) i));
                    half dy = half(srcY) - (half(ky1) + half((float) j));
                    half weight = lanczosWindow(dx, (half) lanczosFA) *
                                  lanczosWindow(dy, (half) lanczosFA);
                    weightSum += weight;
                    for (int c = 0; c < components; ++c) {
                        half clrf = PromoteToHalf(reinterpret_cast<const uint8_t *>(src8 +
                                                                                    clamp(yj, 0,
                                                                                          inputHeight -
                                                                                          1) *
                                                                                    srcStride)[
                                                          clamp(xi, 0, inputWidth - 1) *
                                                          components + c], maxColors);
                        half clr = half(clrf * weight);
                        rgb[c] += clr;
                    }
                }
            }

            for (int c = 0; c < components; ++c) {
                if (weightSum == 0) {
                    dst[x * components + c] = DemoteHalfTo<uint8_t>(rgb[c], maxColors);
                } else {
                    dst[x * components + c] = DemoteHalfTo<uint8_t>(rgb[c] / weightSum, maxColors);
                }
            }
        } else {
            for (int c = 0; c < components; ++c) {
                dst[x * components + c] = reinterpret_cast<const uint8_t *>(src8 + y1 * srcStride)[
                        x1 * components + c];
            }
        }
    }
}

void scaleImageU8(const uint8_t *input,
                  int srcStride,
                  int inputWidth, int inputHeight,
                  uint8_t *output,
                  int dstStride,
                  int outputWidth, int outputHeight,
                  int components,
                  int depth,
                  XSampler option) {
    float xScale = static_cast<float>(inputWidth) / static_cast<float>(outputWidth);
    float yScale = static_cast<float>(inputHeight) / static_cast<float>(outputHeight);

    auto src8 = reinterpret_cast<const uint8_t *>(input);

    float maxColors = std::pow(2.0f, (float) depth) - 1.0f;

    if (inputWidth * inputHeight > 800 * 800) {
        ThreadPool pool;
        std::vector<std::future<void>> results;

        for (int y = 0; y < outputHeight; y++) {
            auto r = pool.enqueue(ScaleRowU8, src8, srcStride, inputWidth, inputHeight, output,
                                  dstStride, outputWidth, components,
                                  option,
                                  xScale, yScale, maxColors, y);
            results.push_back(std::move(r));

        }
        for (auto &result: results) {
            result.wait();
        }
    } else {
        for (int y = 0; y < outputHeight; y++) {
            ScaleRowU8(src8, srcStride, inputWidth, inputHeight, output,
                       dstStride, outputWidth, components,
                       option,
                       xScale, yScale, maxColors, y);
        }
    }
}