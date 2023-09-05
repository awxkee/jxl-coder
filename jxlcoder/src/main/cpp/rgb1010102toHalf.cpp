//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#include "rgb1010102toHalf.h"
#include <cstdint>
#include <halfFloats.h>

// Conversion function from 1010102 to half floats

void convertRGBA1010102toHalf(const uint8_t *src, int srcStride, uint16_t *dst, int dstStride,
                              int width, int height) {
    auto mDstPointer = reinterpret_cast<uint8_t *>(dst);
    auto mSrcPointer = reinterpret_cast<const uint8_t *>(src);

    uint32_t testValue = 0x01020304;
    auto testBytes = reinterpret_cast<uint8_t*>(&testValue);

    bool littleEndian = false;
    if (testBytes[0] == 0x04) {
        littleEndian = true;
    } else if (testBytes[0] == 0x01) {
        littleEndian = false;
    }

    for (int y = 0; y < height; ++y) {

        auto dstPointer = reinterpret_cast<uint8_t *>(mDstPointer);
        auto srcPointer = reinterpret_cast<const uint8_t *>(mSrcPointer);

        for (int x = 0; x < width; ++x) {
            uint32_t rgba1010102 = reinterpret_cast<const uint32_t *>(srcPointer)[0];

            const uint32_t mask = (1u << 10u) - 1u;
            uint32_t b = (rgba1010102) & mask;
            uint32_t g = (rgba1010102 >> 10) & mask;
            uint32_t r = (rgba1010102 >> 20) & mask;
            uint32_t a = (rgba1010102 >> 30) * 0x555;  // Replicate 2 bits to 8 bits.

            // Convert each channel to floating-point values
            float rFloat = static_cast<float>(r) / 1023.0f;
            float gFloat = static_cast<float>(g) / 1023.0f;
            float bFloat = static_cast<float>(b) / 1023.0f;
            float aFloat = static_cast<float>(a) / 1023.0f;

            auto dstCast = reinterpret_cast<uint16_t *>(dstPointer);
            if (littleEndian) {
                dstCast[0] = float_to_half(bFloat);
                dstCast[1] = float_to_half(gFloat);
                dstCast[2] = float_to_half( rFloat);
                dstCast[3] = float_to_half(aFloat);
            } else {
                dstCast[0] = float_to_half(rFloat);
                dstCast[1] = float_to_half(gFloat);
                dstCast[2] = float_to_half( bFloat);
                dstCast[3] = float_to_half(aFloat);
            }

            srcPointer += 4;
            dstPointer += 8;
        }

        mSrcPointer += srcStride;
        mDstPointer += dstStride;
    }
}