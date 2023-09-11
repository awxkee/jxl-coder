//
// Created by Radzivon Bartoshyk on 05/09/2023.
//

#include "Rgba16bitCopy.h"
#include <cstdint>
#include "ThreadPool.hpp"

#if HAVE_NEON

#include <arm_neon.h>

void CopyRGBA16RowNEON(const uint16_t *src, uint16_t *dst, int width) {
    auto srcPtr = reinterpret_cast<const uint16_t *>(src);
    auto dstPtr = reinterpret_cast<uint16_t *>(dst);
    int x;

    for (x = 0; x + 2 < width; x += 2) {
        uint16x8_t neonSrc = vld1q_u16(srcPtr);
        vst1q_u16(dstPtr, neonSrc);
        srcPtr += 8;
        dstPtr += 8;
    }

    for (; x < width; ++x) {
        auto srcPtr64 = reinterpret_cast<const uint64_t *>(srcPtr);
        auto dstPtr64 = reinterpret_cast<uint64_t *>(dstPtr);
        dstPtr64[0] = srcPtr64[0];
        srcPtr += 4;
        dstPtr += 4;
    }
}

void CopyRGBA16NEON(const uint16_t *source, int srcStride,
                    uint16_t *destination, int dstStride,
                    int width, int height) {
    auto src = reinterpret_cast<const uint8_t *>(source);
    auto dst = reinterpret_cast<uint8_t *>(destination);

    ThreadPool pool;
    std::vector<std::future<void>> results;

    for (int y = 0; y < height; ++y) {
        auto r = pool.enqueue(CopyRGBA16RowNEON, reinterpret_cast<const uint16_t *>(src),
                             reinterpret_cast<uint16_t *>(dst), width);
        results.push_back(std::move(r));
        src += srcStride;
        dst += dstStride;
    }

    for (auto &result: results) {
        result.wait();
    }
}

#endif

void CopyRGBA16RowC(const uint16_t *src, uint16_t *dst, int width) {
    auto srcPtr = reinterpret_cast<const uint16_t *>(src);
    auto dstPtr = reinterpret_cast<uint16_t *>(dst);

    for (int x = 0; x < width; ++x) {
        auto srcPtr64 = reinterpret_cast<const uint64_t *>(srcPtr);
        auto dstPtr64 = reinterpret_cast<uint64_t *>(dstPtr);
        dstPtr64[0] = srcPtr64[0];
        srcPtr += 4;
        dstPtr += 4;
    }
}

void CopyRGBA16C(const uint16_t *source, int srcStride,
                 uint16_t *destination, int dstStride,
                 int width, int height) {
    auto src = reinterpret_cast<const uint8_t *>(source);
    auto dst = reinterpret_cast<uint8_t *>(destination);

    ThreadPool pool;
    std::vector<std::future<void>> results;

    for (int y = 0; y < height; ++y) {
        auto r = pool.enqueue(CopyRGBA16RowC, reinterpret_cast<const uint16_t *>(src),
                     reinterpret_cast<uint16_t *>(dst), width);
        results.push_back(std::move(r));
        src += srcStride;
        dst += dstStride;
    }

    for (auto &result: results) {
        result.wait();
    }
}

void CopyRGBA16(const uint16_t *source, int srcStride,
                uint16_t *destination, int dstStride,
                int width, int height) {
#if HAVE_NEON
    CopyRGBA16NEON(source, srcStride, destination, dstStride, width, height);
#else
    CopyRGBA16C(source, srcStride, destination, dstStride, width, height);
#endif
}