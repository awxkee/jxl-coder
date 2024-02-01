//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "processing/BoxBlur.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

#include "BoxBlur.h"
#include <vector>
#include <algorithm>
#include <math.h>
#include <thread>

using namespace std;

HWY_BEFORE_NAMESPACE();

namespace coder::HWY_NAMESPACE {
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
    using hwy::HWY_NAMESPACE::UpperHalf;
    using hwy::HWY_NAMESPACE::ApproximateReciprocal;

    void boxBlurU8HorizontalPass(uint8_t *data, std::vector<uint8_t> &transient, int y, int stride,
                                 int width, int height, int radius) {
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
        const int size = radius * 2 + 1;
        const auto scaleSize = ApproximateReciprocal(Set(dfx4, size));

        auto src = reinterpret_cast<uint8_t *>(data + y * stride);
        auto dst = reinterpret_cast<uint8_t *>(transient.data() + y * stride);
        for (int x = 0; x < width; ++x) {
            VF store = zeros;

            int r = -radius;

            for (; r <= radius; ++r) {
                int pos = clamp((x + r), 0, width - 1) * 4;
                VU pixels = LoadU(du8, &src[pos]);
                store = Add(store, ConvertTo(dfx4, PromoteTo(du32x4, pixels)));
            }

            store = Max(Min(Round(Mul(store, scaleSize)), max255), zeros);
            VU pixelU = DemoteTo(du8, ConvertTo(du32x4, store));
            StoreU(pixelU, du8, dst);

            dst += 4;
        }
    }

    void
    boxBlurU8VerticalPass(uint8_t *data, std::vector<uint8_t> &transient, int y, int stride,
                          int width,
                          int height, int radius) {
//        float rStore = 0.f;
//        float gStore = 0.f;
//        float bStore = 0.f;
//        float aStore = 0.f;
//        const int size = radius * 2 + 1;
//        auto dst = reinterpret_cast<uint8_t *>(data + y * stride);
//        for (int x = 0; x < width; ++x) {
//            rStore = 0.f;
//            gStore = 0.f;
//            bStore = 0.f;
//            aStore = 0.f;
//
//            for (int r = -radius; r <= radius; ++r) {
//                auto src = reinterpret_cast<uint8_t *>(transient.data() +
//                                                       clamp((r + y), 0, height - 1) * stride);
//                int pos = clamp(x, 0, width - 1) * 4;
//                rStore += src[pos + 0];
//                gStore += src[pos + 1];
//                bStore += src[pos + 2];
//                aStore += src[pos + 3];
//            }
//
//            dst[0] = clamp(rStore / size, 0, 255);
//            dst[1] = clamp(gStore / size, 0, 255);
//            dst[2] = clamp(bStore / size, 0, 255);
//            dst[3] = clamp(aStore / size, 0, 255);
//
//            dst += 4;
//        }

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
        const int size = radius * 2 + 1;
        const auto scaleSize = ApproximateReciprocal(Set(dfx4, size));

        auto dst = reinterpret_cast<uint8_t *>(data + y * stride);
        for (int x = 0; x < width; ++x) {
            VF store = zeros;

            int r = -radius;

            for (; r <= radius; ++r) {
                auto src = reinterpret_cast<uint8_t *>(transient.data() +
                                                       clamp((r + y), 0, height - 1) * stride);
                int pos = x * 4;
                VU pixels = LoadU(du8, &src[pos]);
                store = Add(store, ConvertTo(dfx4, PromoteTo(du32x4, pixels)));
            }

            store = Max(Min(Round(Mul(store, scaleSize)), max255), zeros);
            VU pixelU = DemoteTo(du8, ConvertTo(du32x4, store));
            StoreU(pixelU, du8, dst);

            dst += 4;
        }
    }

    void boxBlurU8HWY(uint8_t *data, int stride, int width, int height, int radius) {
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
                    [start, end, width, height, stride, data, radius, &transient]() {
                        for (int y = start; y < end; ++y) {
                            boxBlurU8HorizontalPass(data, transient, y, stride, width, height,
                                                    radius);
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
                            boxBlurU8VerticalPass(data, transient, y, stride, width, height,
                                                  radius);
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
    HWY_EXPORT(boxBlurU8HWY);

    void boxBlurU8(uint8_t *data, int stride, int width, int height, int radius) {
        HWY_DYNAMIC_DISPATCH(boxBlurU8HWY)(data, stride, width, height, radius);
    }
}
#endif