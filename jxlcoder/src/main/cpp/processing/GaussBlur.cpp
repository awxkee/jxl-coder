//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#include "GaussBlur.h"
#include <vector>
#include <thread>

using namespace std;

static vector<float> compute1DGaussianKernel(float width, float sigma) {
    vector<float> kernel(ceil(width));
    int mean = ceil(width) / 2;
    float sum = 0; // For accumulating the kernel values
    for (int x = 0; x < width; x++) {
        kernel[x] = (float) exp(-0.5 * pow((x - mean) / sigma, 2.0));
        sum += kernel[x];
    }
    for (int x = 0; x < width; x++)
        kernel[x] /= sum;
    return std::move(kernel);
}

inline __attribute__((flatten))
int clamp(const int value, const int minValue, const int maxValue) {
    return (value < minValue) ? minValue : ((value > maxValue) ? maxValue : value);
}

void
gaussBlurHorizontal(std::vector<uint8_t> &transient,
                    uint8_t *data, int stride,
                    int y, int width,
                    int height, float radius,
                    vector<float> &kernel) {
    float rStore = 0.f;
    float gStore = 0.f;
    float bStore = 0.f;
    float aStore = 0.f;
    const int iRadius = ceil(radius);
    auto src = reinterpret_cast<uint8_t *>(data + y * stride);
    auto dst = reinterpret_cast<uint8_t *>(transient.data() + y * stride);
    for (int x = 0; x < width; ++x) {
        rStore = 0.f;
        gStore = 0.f;
        bStore = 0.f;
        aStore = 0.f;

        for (int r = -iRadius; r <= iRadius; ++r) {
            int pos = clamp((x + r), 0, width - 1) * 4;
            float weight = kernel[r + iRadius];
            rStore += src[pos + 0] * weight;
            gStore += src[pos + 1] * weight;
            bStore += src[pos + 2] * weight;
            aStore += src[pos + 3] * weight;
        }

        dst[0] = clamp(ceil(rStore), 0, 255);
        dst[1] = clamp(ceil(gStore), 0, 255);
        dst[2] = clamp(ceil(bStore), 0, 255);
        dst[3] = clamp(ceil(aStore), 0, 255);

        dst += 4;
    }
}

void
gaussBlurVertical(std::vector<uint8_t> &transient,
                  uint8_t *data, int stride,
                  int y, int width,
                  int height, float radius,
                  vector<float> &kernel) {
    const int iRadius = ceil(radius);
    float rStore = 0.f;
    float gStore = 0.f;
    float bStore = 0.f;
    float aStore = 0.f;
    auto dst = reinterpret_cast<uint8_t *>(data + y * stride);
    for (int x = 0; x < width; ++x) {
        rStore = 0.f;
        gStore = 0.f;
        bStore = 0.f;
        aStore = 0.f;

        for (int r = -iRadius; r <= iRadius; ++r) {
            auto src = reinterpret_cast<uint8_t *>(transient.data() +
                                                   clamp((r + y), 0, height - 1) * stride);
            int pos = clamp(x, 0, width - 1) * 4;
            float weight = kernel[r + iRadius];
            rStore += src[pos + 0] * weight;
            gStore += src[pos + 1] * weight;
            bStore += src[pos + 2] * weight;
            aStore += src[pos + 3] * weight;
        }

        dst[0] = clamp(ceil(rStore), 0, 255);
        dst[1] = clamp(ceil(gStore), 0, 255);
        dst[2] = clamp(ceil(bStore), 0, 255);
        dst[3] = clamp(ceil(aStore), 0, 255);

        dst += 4;
    }
}


void gaussBlur(uint8_t *data, int stride, int width, int height, float radius, float sigma) {
    vector<float> kernel = compute1DGaussianKernel(radius * 2, sigma);

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
                [start, end, width, height, stride, data, radius, &transient, &kernel]() {
                    for (int y = start; y < end; ++y) {
                        gaussBlurHorizontal(transient, data, stride, y, width, height, radius,
                                            kernel);
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
                [start, end, width, height, stride, data, radius, &transient, &kernel]() {
                    for (int y = start; y < end; ++y) {
                        gaussBlurVertical(transient, data, stride, y, width, height, radius,
                                          kernel);
                    }
                });
    }

    for (std::thread &thread: workers) {
        thread.join();
    }

    transient.clear();
}