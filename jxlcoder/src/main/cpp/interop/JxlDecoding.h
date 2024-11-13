/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 04/09/2023
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

#pragma once

#include <vector>
#include "codestream_header.h"
#include "color_encoding.h"
#include <string>

using namespace std;

class InvalidImageSizeException : public std::exception {
 public:
  InvalidImageSizeException(size_t width, size_t height) {
    this->width = width;
    this->height = height;
  }
  const char *what() {
    std::string errorMessage =
        "Invalid image size exceed allowance, current size w: " + std::to_string(this->width) + ", h: " + std::to_string(this->height);
    return strdup(errorMessage.c_str());
  }
 private:
  size_t width;
  size_t height;
};

bool DecodeJpegXlOneShot(const uint8_t *jxl, size_t size,
                         std::vector<uint8_t> *pixels, size_t *xsize,
                         size_t *ysize, std::vector<uint8_t> *iccProfile,
                         bool *useFloats, uint32_t *bitDepth,
                         bool *alphaPremultiplied, bool allowedFloats,
                         JxlOrientation *jxlOrientation,
                         bool *preferEncoding,
                         JxlColorEncoding *colorEncoding,
                         bool *hasAlphaInOrigin,
                         float* intensityTarget);

bool DecodeBasicInfo(const uint8_t *jxl, size_t size, size_t *xsize, size_t *ysize);
