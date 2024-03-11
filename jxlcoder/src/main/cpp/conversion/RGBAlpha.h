//
// Created by Radzivon Bartoshyk on 06/11/2023.
//

#ifndef JXLCODER_RGBALPHA_H
#define JXLCODER_RGBALPHA_H

#include <cstdint>
#include "ConversionUtils.h"

namespace coder {
void UnpremultiplyRGBA(const uint8_t *JXL_RESTRICT src, uint32_t srcStride,
                       uint8_t *JXL_RESTRICT dst, uint32_t dstStride, uint32_t width,
                       uint32_t height);

void PremultiplyRGBA(const uint8_t *JXL_RESTRICT src, uint32_t srcStride,
                     uint8_t *JXL_RESTRICT dst, uint32_t dstStride, uint32_t width,
                     uint32_t height);
}

#endif //JXLCODER_RGBALPHA_H
