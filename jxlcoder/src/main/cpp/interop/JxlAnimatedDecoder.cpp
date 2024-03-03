//
//  JxlAnimatedDecoder.cpp
//  jxl-coder [https://github.com/awxkee/jxl-coder]
//
//  Created by Radzivon Bartoshyk on 26/10/2023.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

#include "JxlAnimatedDecoder.hpp"

JxlFrame JxlAnimatedDecoder::getFrame(int framePosition) {
    std::lock_guard guard(lock);
    if (framePosition < 0) {
        std::string str = "Frame position must be positive";
        throw AnimatedDecoderError(str);
    }

    if (framePosition >= this->frameInfo.size()) {
        std::string str = "Requested frame index more than frames in the container";
        throw AnimatedDecoderError(str);
    }

    JxlDecoderRewind(dec.get());
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSubscribeEvents(dec.get(),
                                  JXL_DEC_FULL_IMAGE | JXL_DEC_FRAME | JXL_DEC_COLOR_ENCODING)) {
        std::string str = "Cannot subscribe to events";
        throw AnimatedDecoderError(str);
    }
    if (JXL_DEC_SUCCESS != JxlDecoderSetInput(dec.get(), data.data(), data.size())) {
        std::string str = "Set input has failed";
        throw AnimatedDecoderError(str);
    }
    JxlDecoderCloseInput(dec.get());
    JxlDecoderSkipFrames(dec.get(), framePosition);

    if (JXL_DEC_SUCCESS != JxlDecoderSetCoalescing(dec.get(), JXL_TRUE)) {
        std::string str = "Cannot coalesce frames";
        throw AnimatedDecoderError(str);
    }

    int frameTime = 0;
    std::vector<uint8_t> pixels;
    JxlColorEncoding clr;
    bool useColorEncoding = false;
    JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    for (;;) {
        JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
        if (status == JXL_DEC_FRAME) {
            JxlFrameHeader header;
            if (JXL_DEC_SUCCESS != JxlDecoderGetFrameHeader(dec.get(), &header)) {
                std::string str = "Cannot retreive frame header info";
                throw AnimatedDecoderError(str);
            }
            JxlAnimationHeader animation = info.animation;
            if (animation.tps_numerator)
                frameTime = (int) (1000.0 * header.duration * animation.tps_denominator /
                                   animation.tps_numerator);
            else
                frameTime = 0;
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t bufferSize = 0;
            int components = 4;
            if (JXL_DEC_SUCCESS !=
                JxlDecoderImageOutBufferSize(dec.get(), &format, &bufferSize)) {
                std::string str = "Cannot retrieve buffer info size";
                throw AnimatedDecoderError(str);
            }
            int allocationSize = info.xsize * info.ysize * (components) * sizeof(uint8_t);
            if (bufferSize != allocationSize) {
                std::string str = "Buffer size are not valid";
                throw AnimatedDecoderError(str);
            }
            pixels.resize(allocationSize);
            void *pixelsBuffer = (void *) pixels.data();

            if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(),
                                                               &format,
                                                               pixelsBuffer,
                                                               pixels.size())) {
                std::string str = "Cannot decoder buffer info";
                throw AnimatedDecoderError(str);
            }
        } else if (status == JXL_DEC_COLOR_ENCODING) {
            if (JXL_DEC_SUCCESS ==
                JxlDecoderGetColorAsEncodedProfile(dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                                                   &clr)) {
                if (clr.transfer_function == JXL_TRANSFER_FUNCTION_HLG ||
                    clr.transfer_function == JXL_TRANSFER_FUNCTION_PQ ||
                    clr.transfer_function == JXL_TRANSFER_FUNCTION_DCI) {
                    useColorEncoding = true;
                }
            }
        } else if (status == JXL_DEC_FULL_IMAGE || status == JXL_DEC_SUCCESS) {
            // All decoding successfully finished, we are at the end of the file.
            // We must rewind the decoder to get a new frame.
            JxlDecoderRewind(dec.get());
            JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
            JxlDecoderSetInput(dec.get(), data.data(), data.size());
            JxlDecoderCloseInput(dec.get());

            std::vector<uint8_t> iccCopy;
            iccCopy = iccProfile;


            JxlFrame frame = {.pixels = pixels,
                    .iccProfile = iccCopy,
                    .colorEncoding = clr,
                    .hasAlphaInOrigin = info.num_extra_channels > 0 && info.alpha_bits > 0,
                    .preferColorEncoding = useColorEncoding,
                    .duration = frameTime};
            return frame;
        } else {
            std::string str = "Error event has received";
            throw AnimatedDecoderError(str);
        }
    }
}

JxlFrame JxlAnimatedDecoder::nextFrame() {
    std::lock_guard guard(lock);
    int frameTime = 0;
    for (;;) {
        JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
        if (status == JXL_DEC_FULL_IMAGE || status == JXL_DEC_SUCCESS) {
            // All decoding successfully finished, we are at the end of the file.
            // We must rewind the decoder to get a new frame.
            JxlDecoderRewind(dec.get());
            JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
            JxlDecoderSetInput(dec.get(), data.data(), data.size());
            JxlDecoderCloseInput(dec.get());
        } else if (status == JXL_DEC_FRAME) {
            JxlFrameHeader header;
            if (JXL_DEC_SUCCESS != JxlDecoderGetFrameHeader(dec.get(), &header)) {
                std::string str = "Cannot retreive frame header info";
                throw AnimatedDecoderError(str);
            }
            JxlAnimationHeader animation = info.animation;
            if (animation.tps_numerator)
                frameTime = (int) (1000.0 * header.duration * animation.tps_denominator /
                                   animation.tps_numerator);
            else
                frameTime = 0;
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t bufferSize;
            JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
            int components = 4;
            if (JXL_DEC_SUCCESS !=
                JxlDecoderImageOutBufferSize(dec.get(), &format, &bufferSize)) {
                std::string str = "Cannot retrieve buffer info size";
                throw AnimatedDecoderError(str);
            }
            if (bufferSize != info.xsize * info.ysize * (components) * sizeof(uint8_t)) {
                std::string str = "Cannot retrieve buffer info size";
                throw AnimatedDecoderError(str);
            }
            std::vector<uint8_t> pixels;
            pixels.resize(info.xsize * info.ysize * (components) * sizeof(uint8_t));
            void *pixelsBuffer = (void *) pixels.data();

            if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(),
                                                               &format,
                                                               pixelsBuffer,
                                                               pixels.size())) {
                std::string str = "Cannot decoder buffer info";
                throw AnimatedDecoderError(str);
            }
            std::vector<uint8_t> iccCopy;
            iccCopy = iccProfile;
            JxlFrame frame = {.pixels = pixels, .iccProfile = iccCopy, .duration = frameTime};
            return frame;
        } else {
            std::string str = "Error event has received";
            throw AnimatedDecoderError(str);
        }
    }
}
