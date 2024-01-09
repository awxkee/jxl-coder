/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 09/01/2024
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

#ifndef JXLCODER_JXLANIMATEDENCODERCOORDINATOR_H
#define JXLCODER_JXLANIMATEDENCODERCOORDINATOR_H

#include "JxlAnimatedEncoder.hpp"
#include <string>

using namespace std;

class JxlAnimatedEncoderCoordinator {
public:

public:
    JxlAnimatedEncoderCoordinator(JxlAnimatedEncoder *encoder, JxlColorPixelType colorPixelType,
                                  JxlCompressionOption compressionOption, int effort, int quality,
                                  JxlColorMatrix colorSpaceMatrix,
                                  JxlEncodingPixelDataFormat dataPixelFormat) : encoder(encoder),
                                                                                colorPixelType(
                                                                                        colorPixelType),
                                                                                compressionOption(
                                                                                        compressionOption),
                                                                                effort(effort),
                                                                                quality(quality),
                                                                                colorSpaceMatrix(
                                                                                        colorSpaceMatrix),
                                                                                pixelDataFormat(
                                                                                        dataPixelFormat) {

    }

    JxlEncodingPixelDataFormat getDataPixelFormat() {
        return this->pixelDataFormat;
    }

    JxlColorPixelType getColorPixelType() {
        return colorPixelType;
    }

    JxlAnimatedEncoder *getEncoder() {
        return encoder;
    }

    ~JxlAnimatedEncoderCoordinator() {
        if (encoder) {
            delete encoder;
            encoder = nullptr;
        }
    }

private:
    JxlAnimatedEncoder *encoder;
    JxlColorPixelType colorPixelType;
    JxlCompressionOption compressionOption;
    int effort;
    int quality;
    JxlColorMatrix colorSpaceMatrix;
    JxlEncodingPixelDataFormat pixelDataFormat;
};

#endif //JXLCODER_JXLANIMATEDENCODERCOORDINATOR_H
