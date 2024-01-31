//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#include "TiltShift.h"
#include <algorithm>
#include <vector>

float blendColor(const float c, const float c1, const float a) {
    float r = c1 * a + c * (1.0f - a);
    return r;
}

void tiltShift(uint8_t *data, uint8_t *source, std::vector<uint8_t > &blurred, int stride, int width, int height,
               float anchorX, float anchorY, float radius) {
    const float availableDistance = std::sqrt(height * height + width * width);
    const float minDistance = availableDistance * radius;
    const int centerX = width*anchorX;
    const int centerY = height*anchorY;
    for (int y = 0; y < height; ++y) {
        auto src = reinterpret_cast<uint8_t *>(source + y * stride);
        auto dst = reinterpret_cast<uint8_t *>(data + y * stride);
        auto blurredSource = reinterpret_cast<uint8_t *>(blurred.data() + y * stride);
        for (int x = 0; x < width; ++x) {
            const float dx = x - centerX;
            const float dy = y - centerY;
            float currentDistance = std::sqrt(dx*dx + dy*dy);

            auto fraction = std::clamp(currentDistance/minDistance, 0.0f, 1.0f);
            auto blurredA = blurredSource[3];
            auto sourceA = src[3];
            auto newA = blurredA*fraction;

            auto blendedR = blendColor(src[0] / 255.f, blurredSource[0] / 255.f, newA / 255.f);
            auto blendedG = blendColor(src[1] / 255.f, blurredSource[1] / 255.f, newA / 255.f);
            auto blendedB = blendColor(src[2] / 255.f, blurredSource[2] / 255.f, newA / 255.f);
            dst[0] = std::clamp(blendedR*255.f, 0.f, 255.f);
            dst[1] = std::clamp(blendedG*255.f, 0.f, 255.f);
            dst[2] = std::clamp(blendedB*255.f, 0.f, 255.f);
            dst[3] = 255;
            src += 4;
            dst += 4;
            blurredSource += 4;
        }

    }
}