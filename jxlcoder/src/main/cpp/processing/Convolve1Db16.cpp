/*
 *
 *  * MIT License
 *  *
 *  * Copyright (c) 2024 Radzivon Bartoshyk
 *  * aire [https://github.com/awxkee/aire]
 *  *
 *  * Created by Radzivon Bartoshyk on 07/02/24, 6:13 PM
 *  *
 *  * Permission is hereby granted, free of charge, to any person obtaining a copy
 *  * of this software and associated documentation files (the "Software"), to deal
 *  * in the Software without restriction, including without limitation the rights
 *  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  * copies of the Software, and to permit persons to whom the Software is
 *  * furnished to do so, subject to the following conditions:
 *  *
 *  * The above copyright notice and this permission notice shall be included in all
 *  * copies or substantial portions of the Software.
 *  *
 *  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  * SOFTWARE.
 *  *
 *
 */

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "processing/Convolve1Db16.cpp"

#include "hwy/foreach_target.h"
#include "hwy/highway.h"

#include "Convolve1Db16.h"
#include "hwy/highway.h"
#include <thread>
#include "concurrency.hpp"

HWY_BEFORE_NAMESPACE();
namespace coder::HWY_NAMESPACE {

using namespace hwy;
using namespace std;
using namespace hwy::HWY_NAMESPACE;

class Convolve1Db16 {

 public:
  Convolve1Db16(const std::vector<float> &horizontal, const std::vector<float> &vertical) : horizontal(horizontal), vertical(vertical) {

  }

  void convolve(uint16_t *data, const int stride, const int width, const int height) {
    std::vector<uint16_t> transient(stride * height);

    const int threadCount = clamp(min(static_cast<int>(std::thread::hardware_concurrency()),
                                      height * width / (256 * 256)), 1, 12);

    concurrency::parallel_for(threadCount, height, [&](int y) {
      this->horizontalPass(transient, data, stride, y, width, height);
    });;

    concurrency::parallel_for(threadCount, height, [&](int y) {
      this->verticalPass(transient, data, stride, y, width, height);
    });
  }

 private:
  const std::vector<float> horizontal;
  const std::vector<float> vertical;

  void horizontalPass(std::vector<uint16_t> &transient,
                      uint16_t *data, int stride,
                      int y, int width,
                      int height) {
    auto src = reinterpret_cast<hwy::float16_t *>(reinterpret_cast<uint8_t *>(data) + y * stride);
    auto dst = reinterpret_cast<hwy::float16_t *>(reinterpret_cast<uint8_t *>(transient.data()) + y * stride);

    const FixedTag<float32_t, 4> dfx4;
    using VF = Vec<decltype(dfx4)>;
    const VF zeros = Zero(dfx4);
    const FixedTag<hwy::float16_t, 8> df16x8;
    using VFb16 = Vec<decltype(df16x8)>;

    const FixedTag<hwy::float16_t, 4> df16x4;
    using VFb16x4 = Vec<decltype(df16x4)>;

    // Preheat kernel memory to stack
    VF kernelCache[this->horizontal.size()];
    for (int j = 0; j < this->horizontal.size(); ++j) {
      kernelCache[j] = Set(dfx4, this->horizontal[j]);
    }

    const int halfOfKernel = this->horizontal.size() / 2;
    const bool isEven = this->horizontal.size() % 2 == 0;
    const int maxKernel = isEven ? halfOfKernel - 1 : halfOfKernel;

    for (int x = 0; x < width; ++x) {
      VF store = zeros;

      int r = -halfOfKernel;

      for (; r + 4 <= maxKernel && x + r + 4 < width; r += 4) {
        int pos = clamp((x + r), 0, width - 1) * 4;

        VFb16 lane = LoadU(df16x8, &src[pos]);

        VF dWeight = kernelCache[r + halfOfKernel];
        store = Add(store, Mul(PromoteUpperTo(dfx4, lane), dWeight));
        dWeight = kernelCache[r + halfOfKernel + 1];
        store = Add(store, Mul(PromoteLowerTo(dfx4, lane), dWeight));

        pos = clamp((x + r + 2), 0, width - 1) * 4;

        lane = LoadU(df16x8, &src[pos]);

        dWeight = kernelCache[r + halfOfKernel + 2];
        store = Add(store, Mul(PromoteUpperTo(dfx4, lane), dWeight));
        dWeight = kernelCache[r + halfOfKernel + 3];
        store = Add(store, Mul(PromoteLowerTo(dfx4, lane), dWeight));
      }

      for (; r <= maxKernel; ++r) {
        int pos = clamp((x + r), 0, width - 1) * 4;
        VF dWeight = kernelCache[r + halfOfKernel];
        VFb16x4 pixels = LoadU(df16x4, &src[pos]);
        store = Add(store, Mul(PromoteTo(dfx4, pixels), dWeight));
      }

      VFb16x4 pixelU = DemoteTo(df16x4, store);
      StoreU(pixelU, df16x4, dst);

      dst += 4;
    }
  }

  void verticalPass(std::vector<uint16_t> &transient,
                    uint16_t *data, int stride,
                    int y, int width,
                    int height) {

    const FixedTag<float32_t, 4> dfx4;
    using VF = Vec<decltype(dfx4)>;
    const FixedTag<hwy::float16_t, 4> df16x4;
    using VFb16x4 = Vec<decltype(df16x4)>;
    const VF zeros = Zero(dfx4);

    // Preheat kernel memory to stack
    VF kernelCache[vertical.size()];
    for (int j = 0; j < vertical.size(); ++j) {
      kernelCache[j] = Set(dfx4, vertical[j]);
    }

    const int halfOfKernel = this->vertical.size() / 2;
    const bool isEven = this->vertical.size() % 2 == 0;
    const int maxKernel = isEven ? halfOfKernel - 1 : halfOfKernel;

    auto dst = reinterpret_cast<hwy::float16_t *>(reinterpret_cast<uint8_t *>(data) + y * stride);
    for (int x = 0; x < width; ++x) {
      VF store = zeros;

      int r = -halfOfKernel;

      for (; r <= maxKernel; ++r) {
        auto src = reinterpret_cast<hwy::float16_t * > (reinterpret_cast<uint8_t *>(transient.data()) +
            clamp((r + y), 0, height - 1) * stride);
        int pos = clamp(x, 0, width - 1) * 4;
        VF dWeight = kernelCache[r + halfOfKernel];
        VFb16x4 pixels = LoadU(df16x4, &src[pos]);
        store = Add(store, Mul(PromoteTo(dfx4, pixels), dWeight));
      }

      VFb16x4 pixelU = DemoteTo(df16x4, store);
      StoreU(pixelU, df16x4, dst);

      dst += 4;
    }
  }
};

void convolve1DF16(uint16_t *data,
                   int stride,
                   int width,
                   int height,
                   const std::vector<float> &horizontal,
                   const std::vector<float> &vertical) {
  Convolve1Db16 convolution(horizontal, vertical);
  convolution.convolve(data, stride, width, height);
}

}
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace coder {
HWY_EXPORT(convolve1DF16);
void convolve1D(uint16_t *data,
                int stride,
                int width,
                int height,
                const std::vector<float> &horizontal,
                const std::vector<float> &vertical) {
  HWY_DYNAMIC_DISPATCH(convolve1DF16)(data, stride, width, height, horizontal, vertical);
}
}
#endif