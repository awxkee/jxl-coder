/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 08/01/2024
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

#ifndef JXLCODER_JXLDEFINITIONS_H
#define JXLCODER_JXLDEFINITIONS_H

enum JxlColorMatrix {
  MATRIX_ITUR_2020_PQ,
  MATRIX_ITUR_709,
  MATRIX_ITUR_2020,
  MATRIX_DISPLAY_P3,
  MATRIX_SRGB_LINEAR,
  MATRIX_ADOBE_RGB,
  MATRIX_DCI_P3,
  MATRIX_UNKNOWN
};

enum JxlColorPixelType {
  rgb = 1,
  rgba = 2
};

enum JxlCompressionOption {
  loseless = 1,
  loosy = 2
};

enum JxlEncodingPixelDataFormat {
  UNSIGNED_8 = 1,
  BINARY_16 = 2
};

#endif //JXLCODER_JXLDEFINITIONS_H
