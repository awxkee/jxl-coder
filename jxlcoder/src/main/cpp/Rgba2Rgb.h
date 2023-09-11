//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#ifndef JXLCODER_RGBA2RGB_H
#define JXLCODER_RGBA2RGB_H

#include <cstdint>

void rgb8bit2RGB(const uint8_t *src, int srcStride, uint8_t *dst, int dstStride, int height, int width);
void Rgba16bit2RGB(const uint16_t *src, int srcStride, uint16_t *dst, int dstStride, int height,
                   int width);

#if HAVE_NEON
void rgba8bit2RgbNEON(const uint8_t* src, uint8_t* dst, int numPixels);
void rgba16Bit2RgbNEON(const uint16_t* src, int srcStride, uint16_t* dst, int dstStride, int height, int width);
#endif

#endif //JXLCODER_RGBA2RGB_H
