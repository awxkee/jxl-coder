//
// Created by Radzivon Bartoshyk on 13/09/2023.
//

#ifndef JXLCODER_RGBA1010102TOF32_H
#define JXLCODER_RGBA1010102TOF32_H

#include <cstdint>

namespace coder {
    void ConvertRGBA1010102toF32(const uint8_t *src, int srcStride, float *dst, int dstStride,
                                 int width, int height);
}

#endif //JXLCODER_RGBA1010102TOF32_H
