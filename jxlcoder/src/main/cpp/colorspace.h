//
// Created by Radzivon Bartoshyk on 03/09/2023.
//

#ifndef JXLCODER_COLORSPACE_H
#define JXLCODER_COLORSPACE_H

#include <vector>

void convertUseDefinedColorSpace(std::vector <uint8_t> &vector, int stride, int height,
                                 const unsigned char *colorSpace, size_t colorSpaceSize,
                                 bool image16Bits);

#endif //JXLCODER_COLORSPACE_H