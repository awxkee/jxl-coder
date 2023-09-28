//
// Created by Radzivon Bartoshyk on 29/09/2023.
//

#ifndef JXLCODER_XSCALER_H
#define JXLCODER_XSCALER_H

#include <cstdint>

enum XSampler {
    bilinear = 1,
    nearest = 2,
    cubic = 3,
    mitchell = 4
};

void scaleImageFloat16(const uint16_t* input,
                       int srcStride,
                       int inputWidth, int inputHeight,
                       uint16_t* output,
                       int dstStride,
                       int outputWidth, int outputHeight,
                       int components,
                       XSampler option);

void scaleImageU16(const uint16_t* input,
                   int srcStride,
                   int inputWidth, int inputHeight,
                   uint16_t* output,
                   int dstStride,
                   int outputWidth, int outputHeight,
                   int components,
                   int depth,
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

#endif //JXLCODER_XSCALER_H
