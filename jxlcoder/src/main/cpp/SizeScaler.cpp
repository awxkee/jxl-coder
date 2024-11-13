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
                  uint32_t scaledWidth, uint32_t scaledHeight,
                  uint32_t bitDepth,
                  bool alphaPremultiplied,
                  ScaleMode scaleMode,
                  XSampler sampler,
                  bool doesOriginHasAlpha) {
  uint32_t imageWidth = *imageWidthPtr;
  uint32_t imageHeight = *imageHeightPtr;
  if ((scaledHeight != 0 || scaledWidth != 0) && (scaledWidth != 0 && scaledHeight != 0)) {

    int xTranslation = 0, yTranslation = 0;
    int canvasWidth = scaledWidth;
    int canvasHeight = scaledHeight;

    if (scaleMode == Fit || scaleMode == Fill) {
      std::pair<int, int> currentSize(imageWidth, imageHeight);
      if (scaledHeight > 0 && scaledWidth < 0) {
        auto newBounds = ResizeAspectHeight(currentSize, scaledHeight, scaledWidth == -2);
        scaledWidth = newBounds.first;
        scaledHeight = newBounds.second;
      } else if (scaledHeight < 0) {
        auto newBounds = ResizeAspectWidth(currentSize, scaledHeight, scaledHeight == -2);
        scaledWidth = newBounds.first;
        scaledHeight = newBounds.second;
      } else {
        std::pair<int, int> dstSize;
        float scale = 1;
        if (scaleMode == Fill) {
          std::pair<int, int> canvasSize(scaledWidth, scaledHeight);
          dstSize = ResizeAspectFill(currentSize, canvasSize, &scale);
        } else {
          std::pair<int, int> canvasSize(scaledWidth, scaledHeight);
          dstSize = ResizeAspectFit(currentSize, canvasSize, &scale);
        }

        xTranslation = std::max((int) (((float) dstSize.first - (float) canvasWidth) / 2.0f), 0);
        yTranslation = std::max((int) (((float) dstSize.second - (float) canvasHeight) / 2.0f), 0);

        scaledWidth = dstSize.first;
        scaledHeight = dstSize.second;
      }
    }

    uint32_t lineWidth = scaledWidth * static_cast<int>(useFloats ? sizeof(uint16_t) : sizeof(uint8_t)) * 4;
    uint32_t alignment = 64;
    uint32_t padding = (alignment - (lineWidth % alignment)) % alignment;
    uint32_t imdStride = lineWidth + padding;

    std::vector<uint8_t> newImageData(imdStride * scaledHeight);

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

    if (useFloats) {
      weave_scale_u16(reinterpret_cast<const uint16_t *>(rgbaData.data()),
                      imageWidth * 4 * (int) sizeof(uint16_t),
                      imageWidth, imageHeight,
                      reinterpret_cast<uint16_t *>(newImageData.data()),
                      imdStride,
                      scaledWidth, scaledHeight, bitDepth, static_cast<ScalingFunction>(sparkSampler),
                      doesOriginHasAlpha);
    } else {
      weave_scale_u8(reinterpret_cast<const uint8_t *>(rgbaData.data()),
                     (int) imageWidth * 4 * (int) sizeof(uint8_t),
                     imageWidth, imageHeight,
                     reinterpret_cast<uint8_t *>(newImageData.data()),
                     imdStride,
                     scaledWidth, scaledHeight, static_cast<ScalingFunction>(sparkSampler),
                     doesOriginHasAlpha);

    }

    imageWidth = scaledWidth;
    imageHeight = scaledHeight;

    if (xTranslation > 0 || yTranslation > 0) {

      int left = std::max(xTranslation, 0);
      int right = xTranslation + canvasWidth;
      int top = std::max(yTranslation, 0);
      int bottom = yTranslation + canvasHeight;

      int croppedWidth = right - left;
      int croppedHeight = bottom - top;
      int newStride =
          croppedWidth * 4 * (int) (useFloats ? sizeof(uint16_t) : sizeof(uint8_t));
      int srcStride = imdStride;

      std::vector<uint8_t> croppedImage(newStride * croppedHeight);

      uint8_t *dstData = croppedImage.data();
      auto srcData = reinterpret_cast<const uint8_t *>(newImageData.data());

      for (int y = top, yc = 0; y < bottom; ++y, ++yc) {
        int x = 0;
        int xc = 0;
        auto srcRow = reinterpret_cast<const uint8_t *>(srcData + srcStride * y);
        auto dstRow = reinterpret_cast<uint8_t *>(dstData + newStride * yc);
        for (x = left, xc = 0; x < right; ++x, ++xc) {
          if (useFloats) {
            auto dst = reinterpret_cast<uint16_t *>(dstRow + xc * 4 * sizeof(uint16_t));
            auto src = reinterpret_cast<const uint16_t *>(srcRow +
                x * 4 * sizeof(uint16_t));
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
          } else {
            auto dst = reinterpret_cast<uint32_t *>(dstRow);
            auto src = reinterpret_cast<const uint32_t *>(srcRow);
            dst[xc] = src[x];
          }
        }
      }

      imageWidth = croppedWidth;
      imageHeight = croppedHeight;

      rgbaData = croppedImage;
      *stride = newStride;

    } else {
      rgbaData = newImageData;
      *stride = imdStride;
    }

    *imageWidthPtr = imageWidth;
    *imageHeightPtr = imageHeight;

  }
  return true;
}

std::pair<int, int>
ResizeAspectFit(std::pair<int, int> sourceSize, std::pair<int, int> dstSize, float *scale) {
  int sourceWidth = sourceSize.first;
  int sourceHeight = sourceSize.second;
  float xFactor = (float) dstSize.first / (float) sourceSize.first;
  float yFactor = (float) dstSize.second / (float) sourceSize.second;
  float resizeFactor = std::min(xFactor, yFactor);
  *scale = resizeFactor;
  std::pair<int, int> resultSize((int) ((float) sourceWidth * resizeFactor),
                                 (int) ((float) sourceHeight * resizeFactor));
  return resultSize;
}

std::pair<int, int>
ResizeAspectFill(std::pair<int, int> sourceSize, std::pair<int, int> dstSize, float *scale) {
  int sourceWidth = sourceSize.first;
  int sourceHeight = sourceSize.second;
  float xFactor = (float) dstSize.first / (float) sourceSize.first;
  float yFactor = (float) dstSize.second / (float) sourceSize.second;
  float resizeFactor = std::max(xFactor, yFactor);
  *scale = resizeFactor;
  std::pair<int, int> resultSize((int) ((float) sourceWidth * resizeFactor),
                                 (int) ((float) sourceHeight * resizeFactor));
  return resultSize;
}

std::pair<int, int>
ResizeAspectHeight(std::pair<int, int> sourceSize, int maxHeight, bool multipleBy2) {
  int sourceWidth = sourceSize.first;
  int sourceHeight = sourceSize.second;
  float scaleFactor = (float) maxHeight / (float) sourceSize.second;
  std::pair<int, int> resultSize((int) ((float) sourceWidth * scaleFactor),
                                 (int) ((float) sourceHeight * scaleFactor));
  if (multipleBy2) {
    resultSize.first = (resultSize.first / 2) * 2;
    resultSize.second = (resultSize.second / 2) * 2;
  }
  return resultSize;
}

std::pair<int, int>
ResizeAspectWidth(std::pair<int, int> sourceSize, int maxWidth, bool multipleBy2) {
  int sourceWidth = sourceSize.first;
  int sourceHeight = sourceSize.second;
  float scaleFactor = (float) maxWidth / (float) sourceSize.first;
  std::pair<int, int> resultSize((int) ((float) sourceWidth * scaleFactor),
                                 (int) ((float) sourceHeight * scaleFactor));
  if (multipleBy2) {
    resultSize.first = (resultSize.first / 2) * 2;
    resultSize.second = (resultSize.second / 2) * 2;
  }
  return resultSize;
}