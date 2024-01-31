//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#ifndef JXLCODER_BILATERALBLUR_H
#define JXLCODER_BILATERALBLUR_H

#include <cstdint>

void bilateralBlur(uint8_t *data, int stride, int width, int height, float radius, float sigma,
                   float spatialSigma);

#endif //JXLCODER_BILATERALBLUR_H
