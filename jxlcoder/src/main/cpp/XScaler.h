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

#ifndef JXLCODER_XSCALER_H
#define JXLCODER_XSCALER_H

#include <cstdint>

enum XSampler {
    bilinear = 1,
    nearest = 2,
    cubic = 3,
    mitchell = 4,
    lanczos = 5,
    catmullRom = 6,
    hermite = 7,
    bSpline = 8,
    hann = 9,
    bicubic = 10
};

namespace coder {
    void scaleImageFloat16(const uint16_t *input,
                           int srcStride,
                           int inputWidth, int inputHeight,
                           uint16_t *output,
                           int dstStride,
                           int outputWidth, int outputHeight,
                           int components,
                           XSampler option);

    void scaleImageU8(const uint8_t *input,
                      int srcStride,
                      int inputWidth, int inputHeight,
                      uint8_t *output,
                      int dstStride,
                      int outputWidth, int outputHeight,
                      int components,
                      int depth,
                      XSampler option);
}

#endif //JXLCODER_XSCALER_H
