//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#ifndef AVIF_RGBAF16BITNBITU8_H
#define AVIF_RGBAF16BITNBITU8_H

#include <vector>

namespace coder {
    void F32toU8(const float *sourceData, int srcStride,
                 uint8_t *dst, int dstStride, int width,
                 int height, int bitDepth);
}

#endif //AVIF_RGBAF16BITNBITU8_H
