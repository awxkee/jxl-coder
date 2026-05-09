/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 01/01/2023
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

#include "SizeScaler.h"
#include <vector>
#include <string>
#include <jni.h>
#include "JniExceptions.h"
#include "XScaler.h"
#include "Eigen/Eigen"
#include "weaver.h"

bool RescaleImage(std::vector<uint8_t> &rgbaData,
                  JNIEnv *env,
                  uint32_t *stride,
                  bool useFloats,
                  uint32_t *imageWidthPtr, uint32_t *imageHeightPtr,
                  int scaledWidth, int scaledHeight,
                  uint32_t bitDepth,
                  bool alphaPremultiplied,
                  ScaleMode scaleMode,
                  XSampler sampler,
                  bool doesOriginHasAlpha) {
  uint32_t imageWidth = *imageWidthPtr;
  uint32_t imageHeight = *imageHeightPtr;
  if ((scaledHeight != 0 || scaledWidth != 0) && (scaledWidth != 0 && scaledHeight != 0)) {
    ScalingFunction sparkSampler = ScalingFunction::Bilinear;
    switch (sampler) {
      case bilinear: {
        sparkSampler = ScalingFunction::Bilinear;
      }
        break;
      case nearest: {
        sparkSampler = ScalingFunction::Nearest;
      }
        break;
      case cubic: {
        sparkSampler = ScalingFunction::Cubic;
      }
        break;
      case mitchell: {
        sparkSampler = ScalingFunction::Mitchell;
      }
        break;
      case lanczos: {
        sparkSampler = ScalingFunction::Lanczos;
      }
        break;
      case catmullRom: {
        sparkSampler = ScalingFunction::CatmullRom;
      }
        break;
      case hermite: {
        sparkSampler = ScalingFunction::Hermite;
      }
        break;
      case bSpline: {
        sparkSampler = ScalingFunction::BSpline;
      }
        break;
      case hann: {
        sparkSampler = ScalingFunction::Lanczos;
      }
        break;
      case bicubic: {
        sparkSampler = ScalingFunction::Bicubic;
      }
        break;
    }

    WeaveScaleMode mScaleMode = WeaveScaleMode::JustResize;
    switch (scaleMode) {
      case Fit:mScaleMode = WeaveScaleMode::ScaleToFit;
        break;
      case Fill:mScaleMode = WeaveScaleMode::ScaleToFill;
        break;
      case Resize:mScaleMode = WeaveScaleMode::JustResize;
        break;
    }

    if (useFloats) {
      auto scalingResult = weave_scale_u16(reinterpret_cast<const uint16_t *>(rgbaData.data()),
                                           (int) imageWidth * 4 * (int) sizeof(uint16_t),
                                           imageWidth, imageHeight,
                                           scaledWidth, scaledHeight,
                                           bitDepth,
                                           static_cast<ScalingFunction>(sparkSampler),
                                           doesOriginHasAlpha, mScaleMode);
      if (scalingResult.data == nullptr) {
        return false;
      }
      *imageHeightPtr = scalingResult.height;
      *imageWidthPtr = scalingResult.width;
      *stride = scalingResult.stride * 2;
      rgbaData.resize(scalingResult.length * 2);
      memcpy(rgbaData.data(), scalingResult.data, scalingResult.length * 2);
      weave_scaling_result16_free(scalingResult);
      return true;
    } else {
      auto scalingResult = weave_scale_u8(reinterpret_cast<const uint8_t *>(rgbaData.data()),
                                          (int) imageWidth * 4 * (int) sizeof(uint8_t),
                                          imageWidth, imageHeight,
                                          scaledWidth, scaledHeight,
                                          static_cast<ScalingFunction>(sparkSampler),
                                          doesOriginHasAlpha, mScaleMode);
      if (scalingResult.data == nullptr) {
        return false;
      }
      *imageHeightPtr = scalingResult.height;
      *imageWidthPtr = scalingResult.width;
      *stride = scalingResult.stride;
      rgbaData.resize(scalingResult.length);
      memcpy(rgbaData.data(), scalingResult.data, scalingResult.length);
      weave_scaling_result_free(scalingResult);
      return true;
    }
  }
  return true;
}