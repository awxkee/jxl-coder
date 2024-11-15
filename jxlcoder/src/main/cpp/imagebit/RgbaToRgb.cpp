/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 12/11/2024
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

#include "RgbaToRgb.h"

namespace coder {
template<typename T>
void
RgbaToRgb(const T *source,
          uint32_t srcStride,
          T *destination,
          uint32_t dstStride,
          uint32_t width,
          uint32_t height) {
  for (uint32_t y = 0; y < height; ++y) {
    auto data = reinterpret_cast<const T *>(reinterpret_cast<const uint8_t *>(source)
        + y * srcStride);
    auto dst =
        reinterpret_cast<T *>(reinterpret_cast<uint8_t *>(destination) + y * dstStride);
    for (uint32_t x = 0; x < width; ++x) {
      T r = data[0];
      T g = data[1];
      T b = data[2];

      dst[0] = r;
      dst[1] = g;
      dst[2] = b;

      data += 4;
      dst += 3;
    }
  }
}

void
Rgba8ToRgb8(const uint8_t *source,
            uint32_t srcStride,
            uint8_t *destination,
            uint32_t dstStride,
            uint32_t width,
            uint32_t height) {
  RgbaToRgb(source, srcStride, destination, dstStride, width, height);
}

void
Rgba16ToRgb16(const uint16_t *source,
              uint32_t srcStride,
              uint16_t *destination,
              uint32_t dstStride,
              uint32_t width,
              uint32_t height) {
  RgbaToRgb(source, srcStride, destination, dstStride, width, height);
}

}