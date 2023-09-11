//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#include "Rgba2Rgb.h"
#include <cstdint>
#include <vector>
#include "ThreadPool.hpp"

#if HAVE_NEON

#include <arm_neon.h>

void rgba16Bit2RgbNEON(const uint16_t *src, int srcStride, uint16_t *dst, int dstStride, int height,
                       int width) {
    auto rgbData = reinterpret_cast<uint8_t *>(dst);
    auto rgbaData = reinterpret_cast<const uint8_t *>(src);
    for (int y = 0; y < height; ++y) {
        int x;

        auto srcPixels = reinterpret_cast<const uint16_t *>(rgbaData);
        auto dstPixels = reinterpret_cast<uint16_t *>(rgbData);

        for (x = 0; x < width; x += 4) {
            // Load 4 RGBA pixels (16-bit per channel)
            uint16x4x4_t rgba_pixels = vld4_u16(srcPixels);

            // Extract the RGB components (ignore Alpha)
            uint16x4x3_t rgb_pixels;
            rgb_pixels.val[0] = rgba_pixels.val[0]; // Red
            rgb_pixels.val[1] = rgba_pixels.val[1]; // Green
            rgb_pixels.val[2] = rgba_pixels.val[2]; // Blue

            // Store the RGB pixels (16-bit per channel)
            vst3_u16(dstPixels, rgb_pixels);

            srcPixels += 4 * 4;
            dstPixels += 4 * 3;
        }

        for (; x < width; ++x) {
            dstPixels[0] = srcPixels[0];
            dstPixels[1] = srcPixels[1];
            dstPixels[2] = srcPixels[2];

            srcPixels += 3;
            dstPixels += 4;
        }

        rgbData += dstStride;
        rgbaData += srcStride;

    }
}

#endif

void
rgb8bit2RGB(const uint8_t *src, int srcStride, uint8_t *dst, int dstStride, int height, int width) {
    auto rgbData = dst;
    auto rgbaData = src;
    for (int y = 0; y < height; ++y) {

        auto srcPixels = rgbaData;
        auto dstPixels = rgbData;

        for (int x = 0; x < width; ++x) {
            rgbData[0] = rgbaData[0];
            rgbData[1] = rgbaData[1];
            rgbData[2] = rgbaData[2];

            srcPixels += 4;
            dstPixels += 4;
        }

        rgbData += dstStride;
        rgbaData += srcStride;
    }
}

void Rgba16bitToRGB_C(const uint16_t *src, uint16_t *dst, int width) {
    auto srcPixels = reinterpret_cast<const uint16_t *>(src);
    auto dstPixels = reinterpret_cast<uint16_t *>(dst);
    for (int x = 0; x < width; ++x) {
        dstPixels[0] = srcPixels[0];
        dstPixels[1] = srcPixels[1];
        dstPixels[2] = srcPixels[2];

        srcPixels += 4;
        dstPixels += 3;
    }
}

void Rgba16bit2RGB(const uint16_t *src, int srcStride,
                   uint16_t *dst, int dstStride, int height,
                   int width) {
    auto rgbData = reinterpret_cast<uint8_t *>(dst);
    auto rgbaData = reinterpret_cast<const uint8_t *>(src);

    ThreadPool pool;
    std::vector<std::future<void>> results;

    for (int y = 0; y < height; ++y) {
        auto r = pool.enqueue(Rgba16bitToRGB_C,
                     reinterpret_cast<const uint16_t *>(rgbaData + srcStride * y),
                     reinterpret_cast<uint16_t *>(rgbData + dstStride * y), width);
        results.push_back(std::move(r));
    }

    for (auto &result: results) {
        result.wait();
    }
}