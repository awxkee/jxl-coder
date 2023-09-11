//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#ifndef JXLCODER_HALFFLOATS_H
#define JXLCODER_HALFFLOATS_H

#include <cstdint>

#if HAVE_NEON
void RgbaF32ToF16Neon(const float *src, int srcStride, uint16_t *dst, int dstStride,
                      int width, int height) ;
#endif

void RgbaF32ToF16(const float *src, int srcStride, uint16_t *dst, int dstStride, int width,
                  int height);

float half_to_float(const uint16_t x);

uint16_t float_to_half(const float x);

#endif //JXLCODER_HALFFLOATS_H