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

#ifndef JXLCODER_JXLANIMATEDDECODERCOORDINATOR_H
#define JXLCODER_JXLANIMATEDDECODERCOORDINATOR_H

#include "JxlAnimatedDecoder.hpp"
#include "SizeScaler.h"
#include "Support.h"
#include <vector>

using namespace std;

class JxlAnimatedDecoderCoordinator {

public:
    JxlAnimatedDecoderCoordinator(JxlAnimatedDecoder *decoder,
                                  ScaleMode scaleMode,
                                  PreferredColorConfig preferredColorConfig,
                                  XSampler sample) : decoder(decoder), scaleMode(scaleMode),
                                                     preferredColorConfig(preferredColorConfig),
                                                     sampler(sample) {

    }

    int numberOfFrames() {
        return decoder->getNumberOfFrames();
    }

    int frameDuration(int frame) {
        return decoder->getFrameDuration(frame);
    }

    int loopsCount() {
        return decoder->getLoopCount();
    }

    JxlFrame getFrame(int at) {
        return decoder->getFrame(at);
    }

    JxlFrame nextFrame() {
        return decoder->nextFrame();
    }

    ~JxlAnimatedDecoderCoordinator() {
        if (decoder) {
            delete decoder;
            decoder = nullptr;
        }
    }

    ScaleMode getScaleMode() {
        return scaleMode;
    }

    PreferredColorConfig getPreferredColorConfig() {
        return preferredColorConfig;
    }

    XSampler getSampler() {
        return sampler;
    }

    int getWidth() {
        return decoder->getWidth();
    }

    int getHeight() {
        return decoder->getHeight();
    }

    bool isAlphaAttenuated() {
        return decoder->isAlphaAttenuated();
    }

private:
    JxlAnimatedDecoder *decoder;
    ScaleMode scaleMode;
    PreferredColorConfig preferredColorConfig;
    XSampler sampler;
};

#endif //JXLCODER_JXLANIMATEDDECODERCOORDINATOR_H
