//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#ifndef JXLCODER_TILTSHIFT_H
#define JXLCODER_TILTSHIFT_H

#include <cstdint>
#include <vector>

void tiltShift(uint8_t *data, uint8_t *source, std::vector<uint8_t> &blurred, int stride, int width,
               int height, float anchorX, float anchorY, float radius);

#endif //JXLCODER_TILTSHIFT_H
