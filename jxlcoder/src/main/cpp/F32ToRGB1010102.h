//
// Created by Radzivon Bartoshyk on 11/09/2023.
//

#ifndef JXLCODER_F32TORGB1010102_H
#define JXLCODER_F32TORGB1010102_H

#include <vector>

void
F16ToRGBA1010102(std::vector<uint8_t> &data, int srcStride, int *dstStride, int width, int height);

#endif //JXLCODER_F32TORGB1010102_H
