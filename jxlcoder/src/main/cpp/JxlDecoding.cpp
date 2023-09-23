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

#include "JxlDecoding.h"
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "jxl/resizable_parallel_runner.h"
#include "jxl/resizable_parallel_runner_cxx.h"

bool DecodeJpegXlOneShot(const uint8_t *jxl, size_t size,
                         std::vector<uint8_t> *pixels, size_t *xsize,
                         size_t *ysize, std::vector<uint8_t> *iccProfile,
                         bool *useFloats, int* bitDepth,
                         bool *alphaPremultiplied, bool allowedFloats) {
    // Multi-threaded parallel runner.
    auto runner = JxlResizableParallelRunnerMake(nullptr);

    auto dec = JxlDecoderMake(nullptr);
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO |
                                             JXL_DEC_COLOR_ENCODING |
                                             JXL_DEC_FULL_IMAGE)) {
        return false;
    }

    if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec.get(),
                                                       JxlResizableParallelRunner,
                                                       runner.get())) {
        return false;
    }

    JxlBasicInfo info;
    JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    JxlDecoderSetInput(dec.get(), jxl, size);
    JxlDecoderCloseInput(dec.get());

    bool useBitmapHalfFloats = false;

    for (;;) {
        JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());

        if (status == JXL_DEC_ERROR) {
            return false;
        } else if (status == JXL_DEC_NEED_MORE_INPUT) {
            return false;
        } else if (status == JXL_DEC_BASIC_INFO) {
            if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {
                return false;
            }
            *xsize = info.xsize;
            *ysize = info.ysize;
            *alphaPremultiplied = info.alpha_premultiplied;
            *bitDepth = (int)info.bits_per_sample;
            if (info.bits_per_sample > 8 && allowedFloats) {
                *useFloats = true;
                useBitmapHalfFloats = true;
                format = {4, JXL_TYPE_FLOAT, JXL_LITTLE_ENDIAN, 0};
            } else {
                *useFloats = false;
                useBitmapHalfFloats = false;
            }
            JxlResizableParallelRunnerSetThreads(
                    runner.get(),
                    JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
        } else if (status == JXL_DEC_COLOR_ENCODING) {
            // Get the ICC color profile of the pixel data
            size_t iccSize;
            if (JXL_DEC_SUCCESS !=
                JxlDecoderGetICCProfileSize(dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                                            &iccSize)) {
                return false;
            }
            iccProfile->resize(iccSize);
            if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                    dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                    iccProfile->data(), iccProfile->size())) {
                return false;
            }
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t bufferSize;
            if (JXL_DEC_SUCCESS !=
                JxlDecoderImageOutBufferSize(dec.get(), &format, &bufferSize)) {
                return false;
            }
            int stride = (int)*xsize * 4 * (int)(useBitmapHalfFloats ? sizeof(float ) : sizeof(uint8_t));
            if (bufferSize != stride * (*ysize)) {
                return false;
            }
            pixels->resize(stride * (*ysize));
            void *pixelsBuffer = (void *) pixels->data();
            size_t pixelsBufferSize = pixels->size();
            if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(), &format,
                                                               pixelsBuffer,
                                                               pixelsBufferSize)) {
                return false;
            }
        } else if (status == JXL_DEC_FULL_IMAGE) {
            // Nothing to do. Do not yet return. If the image is an animation, more
            // full frames may be decoded. This example only keeps the last one.
        } else if (status == JXL_DEC_SUCCESS) {
            // All decoding successfully finished.
            // It's not required to call JxlDecoderReleaseInput(dec.get()) here since
            // the decoder will be destroyed.
            return true;
        } else {
            return false;
        }
    }
}

bool DecodeBasicInfo(const uint8_t *jxl, size_t size,
                     std::vector<uint8_t> *pixels, size_t *xsize,
                     size_t *ysize) {
    // Multi-threaded parallel runner.
    auto runner = JxlResizableParallelRunnerMake(nullptr);

    auto dec = JxlDecoderMake(nullptr);
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO |
                                             JXL_DEC_COLOR_ENCODING |
                                             JXL_DEC_FULL_IMAGE)) {
        return false;
    }

    if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec.get(),
                                                       JxlResizableParallelRunner,
                                                       runner.get())) {
        return false;
    }

    JxlBasicInfo info;

    JxlDecoderSetInput(dec.get(), jxl, size);
    JxlDecoderCloseInput(dec.get());

    for (;;) {
        JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());

        if (status == JXL_DEC_ERROR) {
            return false;
        } else if (status == JXL_DEC_NEED_MORE_INPUT) {
            return false;
        } else if (status == JXL_DEC_BASIC_INFO) {
            if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {
                return false;
            }
            *xsize = info.xsize;
            *ysize = info.ysize;
            return true;
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            return false;
        } else if (status == JXL_DEC_FULL_IMAGE) {
            return false;
        } else if (status == JXL_DEC_SUCCESS) {
            return false;
        } else {
            return false;
        }
    }
}