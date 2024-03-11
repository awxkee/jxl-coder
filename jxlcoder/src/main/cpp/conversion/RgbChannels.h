//
// Created by Radzivon Bartoshyk on 10/03/2024.
//

#pragma once

#include <cstdint>
#include "ConversionUtils.h"

namespace coder {
template<class T>
void RGBAPickChannel(const T *JXL_RESTRICT src,
                     const uint32_t srcStride,
                     T *JXL_RESTRICT dst,
                     const uint32_t dstStride,
                     const uint32_t width,
                     const uint32_t height,
                     const uint32_t channel);
}