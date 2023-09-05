//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#ifndef JXLCODER_RGB1010102TOHALF_H
#define JXLCODER_RGB1010102TOHALF_H

#include <cstdint>

void convertRGBA1010102toHalf(const uint8_t *src, int srcStride, uint16_t *dst, int dstStride,
                              int width, int height);

#endif //JXLCODER_RGB1010102TOHALF_H
