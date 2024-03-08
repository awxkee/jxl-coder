/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 03/03/2024
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#pragma once

#include <vector>
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "jxl/resizable_parallel_runner.h"
#include "jxl/resizable_parallel_runner_cxx.h"

namespace coder {
class JxlReconstruction {
 public:
  JxlReconstruction(std::vector<uint8_t> &data) : jxlData(data) {

  }

  bool reconstruct() {
    auto runner = JxlResizableParallelRunnerMake(nullptr);

    auto dec = JxlDecoderMake(nullptr);
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_JPEG_RECONSTRUCTION | JXL_DEC_FULL_IMAGE)) {
      return false;
    }
    JxlDecoderSetInput(dec.get(), jxlData.data(), jxlData.size());
    JxlDecoderCloseInput(dec.get());

    JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());

    if (status == JXL_DEC_ERROR) {
      return false;
    }

    jpegData.resize(128);
    if (status == JXL_DEC_JPEG_RECONSTRUCTION) {
      if (JXL_DEC_SUCCESS != JxlDecoderSetJPEGBuffer(dec.get(), jpegData.data(), jpegData.size())) {
        return false;
      }
    }

    size_t used = 0;
    JxlDecoderStatus decProcessResult = JXL_DEC_JPEG_NEED_MORE_OUTPUT;
    while (decProcessResult == JXL_DEC_JPEG_NEED_MORE_OUTPUT) {
      size_t us = JxlDecoderReleaseJPEGBuffer(dec.get());
      used = jpegData.size() - us;
      jpegData.resize(jpegData.size() * 2);
      if (JXL_DEC_SUCCESS != JxlDecoderSetJPEGBuffer(dec.get(), jpegData.data() + used,
                                                     jpegData.size() - used)) {
        return false;
      }
      decProcessResult = JxlDecoderProcessInput(dec.get());
    }

    if (decProcessResult != JXL_DEC_FULL_IMAGE) {
      return false;
    }

    used = jpegData.size() - JxlDecoderReleaseJPEGBuffer(dec.get());
    jpegData.resize(used);
    return true;
  }

  std::vector<uint8_t> getJPEGData() {
    return jpegData;
  }

 private:
  std::vector<uint8_t> jxlData;
  std::vector<uint8_t> jpegData;
};
}