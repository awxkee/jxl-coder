//
// Created by Radzivon Bartoshyk on 09/01/2024.
//

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
