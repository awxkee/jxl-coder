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
#include "JxlDefinitions.h"
#include "encode.h"

/**
 * Compresses the provided pixels.
 *
 * @param pixels input pixels
 * @param xsize width of the input image
 * @param ysize height of the input image
 * @param compressed will be populated with the compressed bytes
 */
bool EncodeJxlOneshot(const std::vector<uint8_t> &pixels, const uint32_t xsize,
                      const uint32_t ysize, std::vector<uint8_t> *compressed,
                      JxlColorPixelType colorspace, JxlCompressionOption compression_option,
                      JxlEncodingPixelDataFormat encodingPixelDataFormat,
                      std::vector<uint8_t> &iccProfile,
                      int effort, int quality, int decodingSpeed,
                      JxlColorEncoding &colorEncoding);
