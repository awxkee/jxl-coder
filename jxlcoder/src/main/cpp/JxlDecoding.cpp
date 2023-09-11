//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#include "JxlDecoding.h"
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "jxl/resizable_parallel_runner.h"
#include "jxl/resizable_parallel_runner_cxx.h"

bool DecodeJpegXlOneShot(const uint8_t *jxl, size_t size,
                         std::vector<uint8_t> *pixels, size_t *xsize,
                         size_t *ysize, std::vector<uint8_t> *icc_profile,
                         bool *useFloats,
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
            size_t icc_size;
            if (JXL_DEC_SUCCESS !=
                JxlDecoderGetICCProfileSize(dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                                            &icc_size)) {
                return false;
            }
            icc_profile->resize(icc_size);
            if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                    dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                    icc_profile->data(), icc_profile->size())) {
                return false;
            }
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t buffer_size;
            if (JXL_DEC_SUCCESS !=
                JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size)) {
                return false;
            }
            if (buffer_size !=
                *xsize * *ysize * 4 * (useBitmapHalfFloats ? sizeof(uint32_t) : sizeof(uint8_t))) {
                return false;
            }
            pixels->resize(*xsize * *ysize * 4 *
                           (useBitmapHalfFloats ? sizeof(uint32_t) : sizeof(uint8_t)));
            void *pixels_buffer = (void *) pixels->data();
            size_t pixels_buffer_size = pixels->size() * sizeof(uint8_t);
            if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(), &format,
                                                               pixels_buffer,
                                                               pixels_buffer_size)) {
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