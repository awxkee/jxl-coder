//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#include "BoxBlur.h"
#include <vector>
#include <algorithm>
#include <math.h>
#include <thread>

using namespace std;

inline __attribute__((flatten))
int clamp(const int value, const int minValue, const int maxValue) {
    return (value < minValue) ? minValue : ((value > maxValue) ? maxValue : value);
}

void boxBlurU8HorizontalPass(uint8_t *data, std::vector<uint8_t> &transient, int y, int stride,
                             int width, int height, int radius) {
    float rStore = 0.f;
    float gStore = 0.f;
    float bStore = 0.f;
    float aStore = 0.f;
    const int size = radius * 2 + 1;
    auto src = reinterpret_cast<uint8_t *>(data + y * stride);
    auto dst = reinterpret_cast<uint8_t *>(transient.data() + y * stride);
    for (int x = 0; x < width; ++x) {

        rStore = 0.f;
        gStore = 0.f;
        bStore = 0.f;
        aStore = 0.f;

        for (int r = -radius; r <= radius; ++r) {
            int pos = clamp((x + r), 0, width - 1) * 4;
            rStore += src[pos + 0];
            gStore += src[pos + 1];
            bStore += src[pos + 2];
            aStore += src[pos + 3];
        }

        dst[0] = clamp(rStore / size, 0, 255);
        dst[1] = clamp(gStore / size, 0, 255);
        dst[2] = clamp(bStore / size, 0, 255);
        dst[3] = clamp(aStore / size, 0, 255);

        dst += 4;
    }
}

void
boxBlurU8VerticalPass(uint8_t *data, std::vector<uint8_t> &transient, int y, int stride, int width,
                      int height, int radius) {
    float rStore = 0.f;
    float gStore = 0.f;
    float bStore = 0.f;
    float aStore = 0.f;
    const int size = radius * 2 + 1;
    auto dst = reinterpret_cast<uint8_t *>(data + y * stride);
    for (int x = 0; x < width; ++x) {
        rStore = 0.f;
        gStore = 0.f;
        bStore = 0.f;
        aStore = 0.f;

        for (int r = -radius; r <= radius; ++r) {
            auto src = reinterpret_cast<uint8_t *>(transient.data() +
                                                   clamp((r + y), 0, height - 1) * stride);
            int pos = clamp(x, 0, width - 1) * 4;
            rStore += src[pos + 0];
            gStore += src[pos + 1];
            bStore += src[pos + 2];
            aStore += src[pos + 3];
        }

        dst[0] = clamp(rStore / size, 0, 255);
        dst[1] = clamp(gStore / size, 0, 255);
        dst[2] = clamp(bStore / size, 0, 255);
        dst[3] = clamp(aStore / size, 0, 255);

        dst += 4;
    }
}

void boxBlurU8(uint8_t *data, int stride, int width, int height, int radius) {
    std::vector<uint8_t> transient(stride * height);
    int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                height * width / (256 * 256)), 1, 12);
    vector<thread> workers;

    auto transientData = transient.data();

    int segmentHeight = height / threadCount;

    for (int i = 0; i < threadCount; i++) {
        int start = i * segmentHeight;
        int end = (i + 1) * segmentHeight;
        if (i == threadCount - 1) {
            end = height;
        }
        workers.emplace_back(
                [start, end, width, height, stride, data, radius, &transient]() {
                    for (int y = start; y < end; ++y) {
                        boxBlurU8HorizontalPass(data, transient, y,stride, width, height, radius);
                    }
                });
    }

    for (std::thread &thread: workers) {
        thread.join();
    }

    workers.clear();

    for (int i = 0; i < threadCount; i++) {
        int start = i * segmentHeight;
        int end = (i + 1) * segmentHeight;
        if (i == threadCount - 1) {
            end = height;
        }
        workers.emplace_back(
                [start, end, width, height, stride, data, radius, &transient]() {
                    for (int y = start; y < end; ++y) {
                        boxBlurU8VerticalPass(data, transient, y,stride,  width, height, radius);
                    }
                });
    }

    for (std::thread &thread: workers) {
        thread.join();
    }

    transient.clear();
}