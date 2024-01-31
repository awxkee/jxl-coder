//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#include "BilateralBlur.h"
#include <vector>

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

static vector<vector<float>> compute2DGaussianKernel(int width, float sigma) {
    int W = 5;
    vector<vector<float>> kernel(width, vector<float>(width));
    double mean = W / 2;
    double sum = 0.0;
    for (int x = 0; x < W; ++x)
        for (int y = 0; y < W; ++y) {
            kernel[x][y] =
                    exp(-0.5 * (pow((x - mean) / sigma, 2.0f) + pow((y - mean) / sigma, 2.0f)))
                    / (2 * M_PI * sigma * sigma);
            sum += kernel[x][y];
        }

    if (sum != 0) {
        for (int x = 0; x < W; ++x)
            for (int y = 0; y < W; ++y)
                kernel[x][y] /= sum;
    }
    return std::move(kernel);
}

inline __attribute__((flatten))
int clamp(const int value, const int minValue, const int maxValue) {
    return (value < minValue) ? minValue : ((value > maxValue) ? maxValue : value);
}

void bilateralBlur(uint8_t *data, int stride, int width, int height, float radius, float sigma, float spatialSigma) {
    vector<float> kernel = compute1DGaussianKernel(radius * 2, sigma);
    vector<float> spatialKernel = compute1DGaussianKernel(radius * 2, spatialSigma);

    float rStore = 0.f;
    float gStore = 0.f;
    float bStore = 0.f;
    float aStore = 0.f;
    std::vector<uint8_t> transient(stride * height);
    const int iRadius = ceil(radius);
    for (int y = 0; y < height; ++y) {
        auto src = reinterpret_cast<uint8_t *>(data + y * stride);
        auto dst = reinterpret_cast<uint8_t *>(transient.data() + y * stride);
        for (int x = 0; x < width; ++x) {

            rStore = 0.f;
            gStore = 0.f;
            bStore = 0.f;
            aStore = 0.f;

            for (int r = -iRadius; r <= iRadius; ++r) {
                int px = clamp((x + r), 0, width - 1);
                int pos = px * 4;
                float weight = kernel[r + iRadius];

                float dx = (float(px) - float(x)) / (iRadius * 2 + 1);
                float dy = (y - float(y)) / (iRadius * 2 + 1);
                float distanceWeight = 1.0f - std::sqrt(
                        dx * dx + dy * dy
                ) * spatialKernel[r + iRadius];
                rStore += src[pos + 0] * weight * distanceWeight;
                gStore += src[pos + 1] * weight * distanceWeight;
                bStore += src[pos + 2] * weight * distanceWeight;
                aStore += src[pos + 3] * weight * distanceWeight;
            }

            dst[0] = clamp(ceil(rStore), 0, 255);
            dst[1] = clamp(ceil(gStore), 0, 255);
            dst[2] = clamp(ceil(bStore), 0, 255);
            dst[3] = clamp(ceil(aStore), 0, 255);

            dst += 4;
        }
    }

    for (int y = 0; y < height; ++y) {
        auto dst = reinterpret_cast<uint8_t *>(data + y * stride);
        for (int x = 0; x < width; ++x) {
            rStore = 0.f;
            gStore = 0.f;
            bStore = 0.f;
            aStore = 0.f;

            for (int r = -iRadius; r <= iRadius; ++r) {
                int py = clamp((r + y), 0, height - 1);
                auto src = reinterpret_cast<uint8_t *>(transient.data() + py * stride);
                int px = clamp(x, 0, width - 1);
                int pos = px * 4;

                auto origSrc = reinterpret_cast<uint8_t *>(transient.data() + y * stride);

                float dx = (float(px) - float(x))/(iRadius * 2 + 1);
                float dy = (float(py) - float(y))/(iRadius * 2 + 1);
                float distanceWeight = 1.0f - std::sqrt(
                        dx * dx + dy * dy
                ) * spatialKernel[r + iRadius];

                float weight = kernel[r + iRadius];
                rStore += src[pos + 0] * weight * distanceWeight;
                gStore += src[pos + 1] * weight * distanceWeight;
                bStore += src[pos + 2] * weight * distanceWeight;
                aStore += src[pos + 3] * weight * distanceWeight;
            }

            dst[0] = clamp(ceil(rStore), 0, 255);
            dst[1] = clamp(ceil(gStore), 0, 255);
            dst[2] = clamp(ceil(bStore), 0, 255);
            dst[3] = clamp(ceil(aStore), 0, 255);

            dst += 4;
        }
    }
}