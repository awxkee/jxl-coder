//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#include "GaussBlur.h"
#include <vector>
#include <thread>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "processing/GaussBlur.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

using namespace std;

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {

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

    using hwy::HWY_NAMESPACE::FixedTag;
    using hwy::HWY_NAMESPACE::Vec;
    using hwy::float32_t;
    using hwy::HWY_NAMESPACE::LoadU;
    using hwy::HWY_NAMESPACE::Set;
    using hwy::HWY_NAMESPACE::Zero;
    using hwy::HWY_NAMESPACE::PromoteTo;
    using hwy::HWY_NAMESPACE::ConvertTo;
    using hwy::HWY_NAMESPACE::Mul;
    using hwy::HWY_NAMESPACE::Add;
    using hwy::HWY_NAMESPACE::Min;
    using hwy::HWY_NAMESPACE::Max;
    using hwy::HWY_NAMESPACE::Round;
    using hwy::HWY_NAMESPACE::DemoteTo;
    using hwy::HWY_NAMESPACE::StoreU;
    using hwy::HWY_NAMESPACE::PromoteLowerTo;
    using hwy::HWY_NAMESPACE::PromoteUpperTo;
    using hwy::HWY_NAMESPACE::LowerHalf;
    using hwy::HWY_NAMESPACE::ExtractLane;
    using hwy::HWY_NAMESPACE::UpperHalf;

    void
    gaussBlurHorizontal(std::vector<uint8_t> &transient,
                        uint8_t *data, int stride,
                        int y, int width,
                        int height, float radius,
                        vector<float> &kernel) {

        const int iRadius = ceil(radius);
        auto src = reinterpret_cast<uint8_t *>(data + y * stride);
        auto dst = reinterpret_cast<uint8_t *>(transient.data() + y * stride);

        const FixedTag<uint8_t, 4> du8;
        const FixedTag<uint8_t, 16> du8x16;
        const FixedTag<uint8_t, 8> du8x8;
        const FixedTag<uint16_t, 8> du16x8;
        const FixedTag<uint32_t, 4> du32x4;
        const FixedTag<float32_t, 4> dfx4;
        using VF = Vec<decltype(dfx4)>;
        using VU = Vec<decltype(du8)>;
        using VU8x16 = Vec<decltype(du8x16)>;
        const auto max255 = Set(dfx4, 255.0f);
        const VF zeros = Zero(dfx4);

        float *kernelData = kernel.data();

        for (int x = 0; x < width; ++x) {
            VF store = zeros;

            int r = -iRadius;

            for (; r <= iRadius; ++r) {
                int pos = clamp((x + r), 0, width - 1) * 4;
                float weight = kernel[r + iRadius];
                VF dWeight = Set(dfx4, weight);
                VU pixels = LoadU(du8, &src[pos]);
                store = Add(store, Mul(ConvertTo(dfx4, PromoteTo(du32x4, pixels)), dWeight));
            }

            store = Max(Min(Round(store), max255), zeros);
            VU pixelU = DemoteTo(du8, ConvertTo(du32x4, store));
            StoreU(pixelU, du8, dst);

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

        const FixedTag<uint8_t, 4> du8;
        const FixedTag<uint8_t, 16> du8x16;
        const FixedTag<uint32_t, 4> du32x4;
        const FixedTag<uint8_t, 8> du8x8;
        const FixedTag<float32_t, 4> dfx4;
        const FixedTag<uint16_t, 8> du16x8;
        using VU8x16 = Vec<decltype(du8x16)>;
        using VF = Vec<decltype(dfx4)>;
        using VU = Vec<decltype(du8)>;
        const auto max255 = Set(dfx4, 255.0f);
        const VF zeros = Zero(dfx4);

        float *kernelData = kernel.data();

        auto dst = reinterpret_cast<uint8_t *>(data + y * stride);
        for (int x = 0; x < width; ++x) {
            VF store = zeros;

            int r = -iRadius;

            for (; r <= iRadius; ++r) {
                auto src = reinterpret_cast<uint8_t *>(transient.data() +
                                                       clamp((r + y), 0, height - 1) * stride);
                int pos = clamp(x, 0, width - 1) * 4;
                float weight = kernel[r + iRadius];
                VF dWeight = Set(dfx4, weight);
                VU pixels = LoadU(du8, &src[pos]);
                store = Add(store, Mul(ConvertTo(dfx4, PromoteTo(du32x4, pixels)), dWeight));
            }

            store = Max(Min(Round(store), max255), zeros);
            VU pixelU = DemoteTo(du8, ConvertTo(du32x4, store));

            StoreU(pixelU, du8, dst);

            dst += 4;
        }
    }


    void
    gaussBlurTear(uint8_t *data, int stride, int width, int height, float radius, float sigma) {
        vector<float> kernel = compute1DGaussianKernel(radius * 2 + 1, sigma);

        std::vector<uint8_t> transient(stride * height);
        int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                    height * width / (256 * 256)), 1, 12);
        vector<thread> workers;

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
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(gaussBlurTear);

    void gaussBlur(uint8_t *data, int stride, int width, int height, float radius, float sigma) {
        HWY_DYNAMIC_DISPATCH(gaussBlurTear)(data, stride, width, height, radius, sigma);
    }
}

#endif