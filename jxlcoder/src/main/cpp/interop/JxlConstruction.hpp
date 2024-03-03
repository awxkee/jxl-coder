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

#include "encode.h"
#include "encode_cxx.h"
#include "thread_parallel_runner.h"
#include "thread_parallel_runner_cxx.h"
#include <vector>

namespace coder {

    class JxlConstruction {
    public:
        JxlConstruction(std::vector<uint8_t> &data) : jpegData(data) {

        }

        bool construct() {
            auto enc = JxlEncoderMake(nullptr);
            auto runner = JxlThreadParallelRunnerMake(nullptr,
                                                      JxlThreadParallelRunnerDefaultNumWorkerThreads());
            if (JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner(enc.get(),
                                                               JxlThreadParallelRunner,
                                                               runner.get())) {
                return false;
            }

            if (JXL_ENC_SUCCESS != JxlEncoderStoreJPEGMetadata(enc.get(), JXL_TRUE)) {
                return false;
            }

            JxlEncoderFrameSettings *frameSettings =
                    JxlEncoderFrameSettingsCreate(enc.get(), nullptr);

            if (JXL_ENC_SUCCESS != JxlEncoderSetFrameLossless(frameSettings, JXL_TRUE)) {
                return false;
            }

            if (JxlEncoderFrameSettingsSetOption(frameSettings,
                                                 JXL_ENC_FRAME_SETTING_EFFORT, 7) != JXL_ENC_SUCCESS) {
                return false;
            }

            if (JXL_ENC_SUCCESS !=
                JxlEncoderFrameSettingsSetOption(frameSettings, JXL_ENC_FRAME_SETTING_DECODING_SPEED, 3)) {
                return false;
            }

            if (JXL_ENC_SUCCESS !=
                JxlEncoderAddJPEGFrame(frameSettings, jpegData.data(), jpegData.size())) {
                return false;
            }

            JxlEncoderCloseInput(enc.get());

            compressed.resize(64);
            uint8_t *next_out = compressed.data();
            size_t avail_out = compressed.size() - (next_out - compressed.data());
            JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
            while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
                process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
                if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
                    size_t offset = next_out - compressed.data();
                    compressed.resize(compressed.size() * 2);
                    next_out = compressed.data() + offset;
                    avail_out = compressed.size() - offset;
                }
            }
            compressed.resize(next_out - compressed.data());
            if (JXL_ENC_SUCCESS != process_result) {
                return false;
            }

            return true;
        }

        std::vector<uint8_t>& getCompressedData() {
            return compressed;
        }

    private:
        const std::vector<uint8_t> jpegData;
        std::vector<uint8_t> compressed;
    };

} // coder
