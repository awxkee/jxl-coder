/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 04/09/2023
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

#include "JxlEncoding.h"
#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include "thread_parallel_runner.h"
#include "thread_parallel_runner_cxx.h"
#include <vector>

bool EncodeJxlOneshot(const std::vector<uint8_t> &pixels, const uint32_t xsize,
                      const uint32_t ysize, std::vector<uint8_t> *compressed,
                      jxl_colorspace colorspace, jxl_compression_option compression_option,
                      bool useFloat16, std::vector<uint8_t> iccProfile, int effort) {
    auto enc = JxlEncoderMake(/*memory_manager=*/nullptr);
    auto runner = JxlThreadParallelRunnerMake(
            /*memory_manager=*/nullptr,
                               JxlThreadParallelRunnerDefaultNumWorkerThreads());
    if (JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner(enc.get(),
                                                       JxlThreadParallelRunner,
                                                       runner.get())) {
        return false;
    }

    JxlPixelFormat pixel_format;
    switch (colorspace) {
        case rgb:
            pixel_format = {3, useFloat16 ? JXL_TYPE_FLOAT16 : JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN,
                            0};
            break;
        case rgba:
            pixel_format = {4, useFloat16 ? JXL_TYPE_FLOAT16 : JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN,
                            0};
            break;
    }

    JxlBasicInfo basic_info;
    JxlEncoderInitBasicInfo(&basic_info);
    basic_info.xsize = xsize;
    basic_info.ysize = ysize;
    basic_info.bits_per_sample = useFloat16 ? 16 : 8;
    basic_info.uses_original_profile = compression_option == loosy ? JXL_FALSE : JXL_TRUE;
    if (useFloat16) {
        basic_info.exponent_bits_per_sample = 5;
    }
    basic_info.num_color_channels = 3;

    if (colorspace == rgba) {
        basic_info.num_extra_channels = 1;
        basic_info.alpha_bits = useFloat16 ? 16 : 8;
        if (useFloat16) {
            basic_info.alpha_exponent_bits = 5;
        }
    }

    if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc.get(), &basic_info)) {
        return false;
    }

    switch (colorspace) {
        case rgb:
            basic_info.num_color_channels = 3;
            break;
        case rgba:
            basic_info.num_color_channels = 4;
            JxlExtraChannelInfo channelInfo;
            JxlEncoderInitExtraChannelInfo(JXL_CHANNEL_ALPHA, &channelInfo);
            channelInfo.bits_per_sample = useFloat16 ? 16 : 8;
            channelInfo.alpha_premultiplied = false;
            if (JXL_ENC_SUCCESS != JxlEncoderSetExtraChannelInfo(enc.get(), 0, &channelInfo)) {
                return false;
            }
            break;
    }

    if (!iccProfile.empty()) {
        if (JXL_ENC_SUCCESS !=
            JxlEncoderSetICCProfile(enc.get(), iccProfile.data(), iccProfile.size())) {
            return false;
        }
    } else {
        JxlColorEncoding color_encoding = {};
        JxlColorEncodingSetToSRGB(&color_encoding, pixel_format.num_channels < 3);
        if (JXL_ENC_SUCCESS !=
            JxlEncoderSetColorEncoding(enc.get(), &color_encoding)) {
            return false;
        }
    }

    JxlEncoderFrameSettings *frame_settings =
            JxlEncoderFrameSettingsCreate(enc.get(), nullptr);

    if (JxlEncoderFrameSettingsSetOption(frame_settings,
                                         JXL_ENC_FRAME_SETTING_EFFORT, effort) != JXL_ENC_SUCCESS) {
        return false;
    }

    if (compression_option == loseless &&
        JXL_ENC_SUCCESS != JxlEncoderSetFrameLossless(frame_settings, JXL_TRUE)) {
        return false;
    }

    if (JXL_ENC_SUCCESS !=
        JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                (void *) pixels.data(),
                                sizeof(uint8_t) * pixels.size())) {
        return false;
    }

    JxlEncoderCloseInput(enc.get());

    compressed->resize(64);
    uint8_t *next_out = compressed->data();
    size_t avail_out = compressed->size() - (next_out - compressed->data());
    JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
    while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
        process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
        if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
            size_t offset = next_out - compressed->data();
            compressed->resize(compressed->size() * 2);
            next_out = compressed->data() + offset;
            avail_out = compressed->size() - offset;
        }
    }
    compressed->resize(next_out - compressed->data());
    if (JXL_ENC_SUCCESS != process_result) {
        return false;
    }

    return true;
}
