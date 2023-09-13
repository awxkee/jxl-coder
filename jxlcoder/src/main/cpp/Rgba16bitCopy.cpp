//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#include "Rgba16bitCopy.h"
#include <cstdint>
#include "ThreadPool.hpp"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "Rgba16bitCopy.cpp"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();

namespace coder {
    namespace HWY_NAMESPACE {
        namespace hn = hwy::HWY_NAMESPACE;
        using hwy::HWY_NAMESPACE::FixedTag;
        using hwy::HWY_NAMESPACE::Load;
        using hwy::HWY_NAMESPACE::Store;

        void CopyRGBA16RowHWY(const uint16_t *HWY_RESTRICT data, uint16_t * HWY_RESTRICT dst, int width) {
            const FixedTag<uint16_t, 8> du;
            using V = hn::Vec<decltype(du)>;
            int x = 0;
            int pixels = 2;
            for (x = 0; x + pixels < width; x += pixels) {
                V color = Load(du, data);
                Store(color, du, dst);
                data += 4 * pixels;
                dst += 4 * pixels;
            }

            for (; x < width; ++x) {
                auto srcPtr64 = reinterpret_cast<const uint64_t *>(data);
                auto dstPtr64 = reinterpret_cast<uint64_t *>(dst);
                dstPtr64[0] = srcPtr64[0];
                data += 4;
                dst += 4;
            }
        }

        void CopyRGBA16(const uint16_t *HWY_RESTRICT source, int srcStride,
                        uint16_t *HWY_RESTRICT destination, int dstStride,
                        int width, int height) {
            auto src = reinterpret_cast<const uint8_t *>(source);
            auto dst = reinterpret_cast<uint8_t *>(destination);
            int minimumTilingAreaSize = 850 * 850;
            int currentAreaSize = width * height;

            if (minimumTilingAreaSize > currentAreaSize) {
                for (int y = 0; y < height; ++y) {
                    CopyRGBA16RowHWY(reinterpret_cast<const uint16_t *HWY_RESTRICT>(src),
                                     reinterpret_cast<uint16_t *HWY_RESTRICT>(dst), width);
                    src += srcStride;
                    dst += dstStride;
                }

            } else {
                ThreadPool pool;
                std::vector<std::future<void>> results;

                for (int y = 0; y < height; ++y) {
                    auto r = pool.enqueue(CopyRGBA16RowHWY,
                                          reinterpret_cast<const uint16_t *HWY_RESTRICT>(src),
                                          reinterpret_cast<uint16_t *HWY_RESTRICT>(dst), width);
                    results.push_back(std::move(r));
                    src += srcStride;
                    dst += dstStride;
                }

                for (auto &result: results) {
                    result.wait();
                }
            }
        }

    }
}

HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
    HWY_EXPORT(CopyRGBA16);
    HWY_DLLEXPORT void CopyRGBA16(const uint16_t *HWY_RESTRICT source, int srcStride,
                                  uint16_t *HWY_RESTRICT destination, int dstStride,
                                  int width, int height) {
        HWY_DYNAMIC_DISPATCH(CopyRGBA16)(source, srcStride, destination, dstStride, width, height);
    }
}
#endif