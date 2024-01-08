//
// Created by Radzivon Bartoshyk on 08/01/2024.
//

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
                                  int scaleWidth, int scaleHeight,
                                  ScaleMode scaleMode,
                                  PreferredColorConfig preferredColorConfig,
                                  XSampler sample) : decoder(decoder), scaleMode(scaleMode),
                                                     preferredColorConfig(preferredColorConfig),
                                                     sampler(sample),
                                                     scaleHeight(scaleHeight),
                                                     scaleWidth(scaleWidth) {

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

    int getScaleWidth() {
        return scaleWidth;
    }

    int getScaleHeight() {
        return scaleHeight;
    }

    bool isAlphaAttenuated() {
        return decoder->isAlphaAttenuated();
    }

private:
    int scaleWidth, scaleHeight;
    JxlAnimatedDecoder *decoder;
    ScaleMode scaleMode;
    PreferredColorConfig preferredColorConfig;
    XSampler sampler;
};

#endif //JXLCODER_JXLANIMATEDDECODERCOORDINATOR_H
