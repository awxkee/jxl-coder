//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#include "MedianBlur.h"
#include <vector>
#include <thread>

using namespace std;

inline __attribute__((flatten))
uint8_t getMedian(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> copy = data;
    std::nth_element(copy.begin(), copy.begin() + copy.size() / 2, copy.end());
    return copy[copy.size() / 2];
}

void medianBlurU8Runner(std::vector<uint8_t> &transient, uint8_t *data, int stride, int width,
                        int y, int radius, int height) {
    const int size = radius * 2 + 1;
    auto dst = reinterpret_cast<uint8_t *>(transient.data() + y * stride);
    for (int x = 0; x < width; ++x) {

        std::vector<uint8_t> rStore;
        std::vector<uint8_t> gStore;
        std::vector<uint8_t> bStore;
        std::vector<uint8_t> aStore;

        for (int j = -radius; j <= radius; ++j) {
            for (int i = -radius; i <= radius; ++i) {
                auto src = reinterpret_cast<uint8_t *>(data +
                                                       clamp(y + j, 0, height - 1) * stride);
                int pos = clamp((x + i), 0, width - 1) * 4;
                rStore.insert(rStore.end(), src[pos + 0]);
                gStore.insert(gStore.end(), src[pos + 1]);
                bStore.insert(bStore.end(), src[pos + 2]);
                aStore.insert(aStore.end(), src[pos + 3]);
            }
        }

        dst[0] = getMedian(rStore);
        dst[1] = getMedian(gStore);
        dst[2] = getMedian(bStore);
        dst[3] = getMedian(aStore);

        dst += 4;
    }
}

void medianBlurU8(uint8_t *data, int stride, int width, int height, int radius) {
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
                        medianBlurU8Runner(transient, data, stride, width, y, radius, height);
                    }
                });
    }

    for (std::thread &thread: workers) {
        thread.join();
    }

    std::copy(transient.begin(), transient.end(), data);
}