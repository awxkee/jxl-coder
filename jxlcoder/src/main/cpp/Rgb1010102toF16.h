//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#ifndef JXLCODER_RGB1010102TOF16_H
#define JXLCODER_RGB1010102TOF16_H

#include <cstdint>

namespace coder {
    void ConvertRGBA1010102toF16(const uint8_t *src, int srcStride, uint16_t *dst, int dstStride,
                                 int width, int height);
}

#endif //JXLCODER_RGB1010102TOF16_H
