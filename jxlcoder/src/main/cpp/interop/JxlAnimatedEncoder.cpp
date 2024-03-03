//
//  JxlAnimatedEncoder.cpp
//  JxclCoder [https://github.com/awxkee/jxl-coder-swift]
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

#include "JxlAnimatedEncoder.hpp"

void JxlAnimatedEncoder::addFrame(std::vector<uint8_t>& data, int frameTime) {
    std::lock_guard guard(lock);

    addedFrames += 1;

    JxlEncoderInitFrameHeader(&header);
    header.timecode = 0;
    header.duration = frameTime;
    header.is_last = false;
    header.layer_info.crop_x0 = 0;
    header.layer_info.crop_y0 = 0;
    header.layer_info.xsize = width;
    header.layer_info.ysize = height;

    if (JXL_ENC_SUCCESS != JxlEncoderSetFrameHeader(frameSettings, &header)) {
        std::string str = "Set frame header has failed";
        throw AnimatedEncoderError(str);
    }

    if (JXL_ENC_SUCCESS !=
        JxlEncoderAddImageFrame(frameSettings, &pixelFormat,
                                (void *) data.data(),
                                sizeof(uint8_t) * data.size())) {
        std::string str = "Encoding frame has failed";
        throw AnimatedEncoderError(str);
    }
}

void JxlAnimatedEncoder::encode(std::vector<uint8_t>& dst) {
    std::lock_guard guard(lock);
    if (addedFrames == 0) {
        std::string str = "Cannot compress empty animation";
        throw AnimatedEncoderError(str);
    }
    JxlEncoderCloseFrames(enc.get());

    dst.resize(64);
    uint8_t *nextOut = dst.data();
    size_t availOut = dst.size() - (nextOut - dst.data());
    JxlEncoderStatus processResult = JXL_ENC_NEED_MORE_OUTPUT;
    while (processResult == JXL_ENC_NEED_MORE_OUTPUT) {
        processResult = JxlEncoderProcessOutput(enc.get(), &nextOut, &availOut);
        if (processResult == JXL_ENC_NEED_MORE_OUTPUT) {
            size_t offset = nextOut - dst.data();
            dst.resize(dst.size() * 2);
            nextOut = dst.data() + offset;
            availOut = dst.size() - offset;
        }
    }
    dst.resize(nextOut - dst.data());
    if (JXL_ENC_SUCCESS != processResult) {
        std::string str = "Encoding image has failed";
        throw AnimatedEncoderError(str);
    }
}

JxlAnimatedEncoder::~JxlAnimatedEncoder() {

}
