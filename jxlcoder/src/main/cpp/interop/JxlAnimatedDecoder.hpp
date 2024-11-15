//
//  JxlAnimatedDecoder.hpp
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

#ifndef JxlAnimatedDecoder_hpp
#define JxlAnimatedDecoder_hpp

#ifdef __cplusplus

#include <cstdio>
#include <string>
#include <vector>
#include "decode.h"
#include "decode_cxx.h"
#include "resizable_parallel_runner.h"
#include "resizable_parallel_runner_cxx.h"
#include <thread>
#include "conversion/HalfFloats.h"

class AnimatedDecoderError : public std::exception {
 public:
  AnimatedDecoderError(const std::string &message) : errorMessage(message) {}

  const char *what() const noexcept override {
    return errorMessage.c_str();
  }

 private:
  std::string errorMessage;
};

struct JxlFrame {
  std::vector<uint8_t> pixels;
  std::vector<uint8_t> iccProfile;
  JxlColorEncoding colorEncoding;
  bool hasAlphaInOrigin;
  bool preferColorEncoding;
  int duration;
};

struct JxlFrameInfo {
  int duration;
};

class JxlAnimatedDecoder {
 public:
  JxlAnimatedDecoder(std::vector<uint8_t> &src) {
    this->data = src;

    if (JXL_SIG_INVALID == JxlSignatureCheck(src.data(), src.size())) {
      std::string str = "Not an JXL image";
      throw AnimatedDecoderError(str);
    }

    runner = JxlResizableParallelRunnerMake(nullptr);

    dec = JxlDecoderMake(nullptr);
    if (!dec) {
      std::string str = "Cannot create decoder";
      throw AnimatedDecoderError(str);
    }
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO |
            JXL_DEC_COLOR_ENCODING |
            JXL_DEC_FULL_IMAGE |
            JXL_DEC_FRAME)) {
      std::string str = "Cannot subscribe to decoder events";
      throw AnimatedDecoderError(str);
    }

    if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec.get(),
                                                       JxlResizableParallelRunner,
                                                       runner.get())) {
      std::string str = "Cannot attach parallel runner to decoder";
      throw AnimatedDecoderError(str);
    }

    if (JXL_DEC_SUCCESS != JxlDecoderSetCoalescing(dec.get(), JXL_FALSE)) {
      std::string str = "Cannot coalesce frames";
      throw AnimatedDecoderError(str);
    }

    JxlDecoderSetInput(dec.get(), data.data(), data.size());
    JxlDecoderCloseInput(dec.get());

    for (;;) {
      JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
      if (status == JXL_DEC_ERROR) {
        std::string str = "Cannot retreive basic info";
        throw AnimatedDecoderError(str);
      } else if (status == JXL_DEC_BASIC_INFO) {
        if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {
          std::string str = "Cannot retreive basic info";
          throw AnimatedDecoderError(str);
        }
        loopCount = info.have_animation ? info.animation.num_loops : -1;
        denom = info.have_animation ? info.animation.tps_denominator : 1;
        numer = info.have_animation ? info.animation.tps_numerator : 1;
        alphaPremultiplied = info.alpha_premultiplied;

        uint64_t maxSize = std::numeric_limits<int32_t>::max();
        uint64_t
            currentSize = static_cast<uint64_t >(info.xsize) * static_cast<uint64_t >(info.ysize) * 4;
        if (currentSize >= maxSize) {
          std::string errorMessage =
              "Invalid image size exceed allowance, current size w: " + std::to_string(info.xsize) + ", h: " + std::to_string(info.ysize);
          throw AnimatedDecoderError(strdup(errorMessage.c_str()));
        }

        JxlResizableParallelRunnerSetThreads(
            runner.get(),
            JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
      } else if (status == JXL_DEC_FULL_IMAGE) {
        // All decoding successfully finished, we are at the end of the file.
        // We must rewind the decoder to get a new frame.
        JxlDecoderRewind(dec.get());
        JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
        JxlDecoderSetInput(dec.get(), data.data(), data.size());
        JxlDecoderCloseInput(dec.get());
        break;
      } else if (status == JXL_DEC_FRAME) {
        JxlFrameHeader header;
        if (JXL_DEC_SUCCESS != JxlDecoderGetFrameHeader(dec.get(), &header)) {
          std::string str = "Cannot retreive frame header info";
          throw AnimatedDecoderError(str);
        }
        JxlAnimationHeader animation = info.animation;
        int frameTime;
        if (animation.tps_numerator)
          frameTime = std::roundf(1000.0f * static_cast<float>(header.duration) * static_cast<float>(animation.tps_denominator)
                                      / static_cast<float>(animation.tps_numerator));
        else
          frameTime = 0;
        JxlFrameInfo fInfo = {.duration = frameTime};
        this->frameInfo.push_back(fInfo);
      } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
        if (JXL_DEC_SUCCESS != JxlDecoderSkipCurrentFrame(dec.get())) {
          std::string str = "Cannot properly resolve animation info";
          throw AnimatedDecoderError(str);
        }
      } else if (status == JXL_DEC_COLOR_ENCODING) {
        size_t iccSize;
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetICCProfileSize(dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                                        &iccSize)) {
          std::string str = "Cannot retreive color info";
          throw AnimatedDecoderError(str);
        }
        iccProfile.resize(iccSize);
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetColorAsICCProfile(dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                                           iccProfile.data(), iccProfile.size())) {
          std::string str = "Cannot retrieve color icc profile";
          throw AnimatedDecoderError(str);
        }
      } else if (status == JXL_DEC_SUCCESS) {
        JxlDecoderRewind(dec.get());
        JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE);
        JxlDecoderSetInput(dec.get(), data.data(), data.size());
        JxlDecoderCloseInput(dec.get());
        break;
      }
    }
  }

  JxlFrame nextFrame();

  JxlFrame getFrame(int at);

  [[nodiscard]] uint32_t getLoopCount() {
    return loopCount;
  }

  [[nodiscard]] uint32_t getWidth() const {
    return info.xsize;
  }

  [[nodiscard]] uint32_t getHeight() const {
    return info.ysize;
  }

  int getNumberOfFrames() {
    return static_cast<int>(frameInfo.size());
  }

  [[nodiscard]] bool isAlphaAttenuated() const {
    return alphaPremultiplied;
  }

  int getFrameDuration(int frame) {
    std::lock_guard guard(lock);
    if (frame < 0) {
      return 0;
    }

    if (frame >= this->frameInfo.size()) {
      return 0;
    }
    JxlFrameInfo fInfo = this->frameInfo[frame];
    return fInfo.duration;
  }

 private:
  std::vector<uint8_t> data;
  std::vector<uint8_t> iccProfile;
  std::vector<JxlFrameInfo> frameInfo;
  JxlDecoderPtr dec;
  JxlBasicInfo info;
  bool alphaPremultiplied;
  int loopCount;
  int denom;
  int numer;
  JxlResizableParallelRunnerPtr runner;
  std::mutex lock;
};

#endif

#endif /* JxlAnimatedDecoder_hpp */
