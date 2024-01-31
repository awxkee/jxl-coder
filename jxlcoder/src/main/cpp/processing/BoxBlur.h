//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#ifndef JXLCODER_BOXBLUR_H
#define JXLCODER_BOXBLUR_H

#include <cstdint>

void boxBlurU8(uint8_t* data, int stride, int width, int height, int radius);

#endif //JXLCODER_BOXBLUR_H
