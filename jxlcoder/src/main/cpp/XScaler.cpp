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

using namespace half_float;
using namespace std;

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
inline T CubicBSpline(T t) {
    T absX = abs(t);
    if (absX <= 1.0) {
        T doubled = absX * absX;
        T tripled = doubled * absX;
        return T(2.0 / 3.0) - doubled + (T(0.5) * tripled);
    } else if (absX <= 2.0) {
        return ((T(2.0) - absX) * (T(2.0) - absX) * (T(2.0) - absX)) / T(6.0);
    } else {
        return T(0.0);
    }
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

template<typename T>
T CubicBSpline(T d, T p0, T p1, T p2, T p3) {
    //    T t2 = t * t;
    //    T a0 = -T(0.5) * p0 + T(1.5) * p1 - T(1.5) * p2 + T(0.5) * p3;
    //    T a1 = p0 - T(2.5) * p1 + T(2.0f) * p2 - T(0.5) * p3;
    //    T a2 = -T(0.5) * p0 + T(0.5) * p2;
    //    T a3 = p1;
    //    return (a0 * t * t2 + a1 * t2 + a2 * t + a3);
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

    // Map x to the range [-pi, pi]
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
inline T CatmullRom(T x) {
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

    if (x < T(1.0)) {
        T doublePower = x * x;
        T triplePower = doublePower * x;
        return T(0.5) * (((T(2.0) * p1) +
                          (-p0 + p2) * x +
                          (T(2.0) * p0 - T(5.0) * p1 + T(4.0) * p2 - p3) * doublePower +
                          (-p0 + T(3.0) * p1 - T(3.0) * p2 + p3) * triplePower));
    }
    return T(0.0);
}

typedef float (*KernelSample4Func)(float, float, float, float, float);

typedef float (*KernelWindow2Func)(float, const float);

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

            float dx((float) x2 - (float) x1);
            float dy((float) y2 - (float) y1);

            float invertDx = float(1.0f) - dx;
            float invertDy = float(1.0f) - dy;

            auto row1 = reinterpret_cast<const uint16_t *>(src8 + y1 * srcStride);
            auto row2 = reinterpret_cast<const uint16_t *>(src8 + y2 * srcStride);

            for (int c = 0; c < components; ++c) {
                float c1 = castU16(row1[x1 * components + c]) * invertDx * invertDy;
                float c2 = castU16(row1[x2 * components + c]) * dx * invertDy;
                float c3 = castU16(row2[x1 * components + c]) * invertDx * dy;
                float c4 = castU16(row2[x2 * components + c]) * dx * dy;

                float result = c1 + c2 + c3 + c4;
                dst16[x * components + c] = half(result).data_;
            }
        } else if (option == cubic || option == mitchell || option == bSpline ||
                   option == catmullRom || option == hermite) {
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

            auto row = reinterpret_cast<const uint16_t *>(src8 + clamp(yj, 0, inputHeight - 1) *
                                                                 srcStride);
            auto rowy1 = reinterpret_cast<const uint16_t *>(src8 +
                                                            clamp(yj + 1, 0, inputHeight - 1) *
                                                            srcStride);

            for (int c = 0; c < components; ++c) {
                float clr = sampler(srcX - (float) xi,
                                    (float) castU16(
                                            row[clamp(xi, 0, inputWidth - 1) * components +
                                                c]),
                                    (float) castU16(
                                            rowy1[clamp(xi + 1, 0, inputWidth - 1) *
                                                  components +
                                                  c]),
                                    (float) castU16(
                                            row[clamp(xi + 1, 0, inputWidth - 1) * components +
                                                c]),
                                    (float) castU16(
                                            rowy1[clamp(xi + 1, 0, inputWidth - 1) *
                                                  components +
                                                  c]));
                dst16[x * components + c] = half(clr).data_;
            }
        } else if (option == lanczos || option == hann) {
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
                                                                  clamp(yj, 0, inputHeight - 1) *
                                                                  srcStride);

                    for (int c = 0; c < components; ++c) {
                        half clrf = castU16(row[clamp(xi, 0, inputWidth - 1) * components + c]);
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
        } else {
            auto row = reinterpret_cast<const uint16_t *>(src8 + y1 * srcStride);
            for (int c = 0; c < components; ++c) {
                dst16[x * components + c] = row[x1 * components + c];
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
            int x2 = min(x1 + 1, inputWidth - 1);
            int y2 = min(y1 + 1, inputHeight - 1);

            float dx = (float) x2 - (float) x1;
            float dy = (float) y2 - (float) y1;

            auto row1 = reinterpret_cast<const uint8_t *>(src8 + y1 * srcStride);
            auto row2 = reinterpret_cast<const uint8_t *>(src8 + y2 * srcStride);

            float invertDx = float(1.0f) - dx;
            float invertDy = float(1.0f) - dy;

            for (int c = 0; c < components; ++c) {
                float c1 = static_cast<float>(row1[x1 * components + c]) * invertDx * invertDy;
                float c2 = static_cast<float>(row1[x2 * components + c]) * dx * invertDy;
                float c3 = static_cast<float>(row2[x1 * components + c]) * invertDx * dy;
                float c4 = static_cast<float>(row2[x2 * components + c]) * dx * dy;

                float result = (c1 + c2 + c3 + c4);
                float f = result;
                f = clamp(round(f), 0.0f, maxColors);
                dst[x * components + c] = static_cast<uint8_t>(f);

            }
        } else if (option == cubic || option == mitchell || option == bSpline ||
                   option == catmullRom || option == hermite) {
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
                                                         clamp(yj, 0, inputHeight - 1) * srcStride);
            auto rowy1 = reinterpret_cast<const uint8_t *>(src8 +
                                                           clamp(yj + 1, 0, inputHeight - 1) *
                                                           srcStride);

            for (int c = 0; c < components; ++c) {
                float weight = sampler(srcX - (float) xi,
                                       static_cast<float>(row[
                                               clamp(xi, 0, inputWidth - 1) * components + c]),
                                       static_cast<float>(rowy1[clamp(xi + 1, 0, inputWidth - 1) *
                                                                components + c]),
                                       static_cast<float>(row[clamp(xi + 1, 0, inputWidth - 1) *
                                                              components + c]),
                                       static_cast<float>(rowy1[clamp(xi + 1, 0, inputWidth - 1) *
                                                                components + c]));
                dst[x * components + c] = static_cast<uint8_t>(clamp(round(weight), 0.0f,
                                                                     maxColors));
            }
        } else if (option == lanczos || option == hann) {
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
                                                                 clamp(yj, 0, inputHeight - 1) *
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
                    dst[x * components + c] = static_cast<uint8_t>(clamp(round(rgb[c]), 0.0f,
                                                                         maxColors));
                } else {
                    dst[x * components + c] = static_cast<uint8_t>(clamp(round(rgb[c] / weightSum),
                                                                         0.0f, maxColors));
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