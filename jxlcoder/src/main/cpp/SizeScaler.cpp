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
#include "processing/Convolve1D.h"
#include "processing/Convolve1Db16.h"
#include "Eigen/Eigen"

static std::vector<float> compute1DGaussianKernel(int width, float sigma) {
  std::vector<float> kernel(ceil(width));
  int mean = ceil(width) / 2;
  float sum = 0;
  const float scale = 1.f / (std::sqrtf(2 * M_PI) * sigma);
  for (int x = 0; x < width; x++) {
    kernel[x] = std::expf(-0.5 * std::powf((x - mean) / sigma, 2.0)) * scale;
    sum += kernel[x];
  }
  for (int x = 0; x < width; x++)
    kernel[x] /= sum;
  return std::move(kernel);
}

bool RescaleImage(std::vector<uint8_t> &rgbaData,
                  JNIEnv *env,
                  int *stride,
                  bool useFloats,
                  int *imageWidthPtr, int *imageHeightPtr,
                  int scaledWidth, int scaledHeight,
                  bool alphaPremultiplied,
                  ScaleMode scaleMode,
                  XSampler sampler) {
  int imageWidth = *imageWidthPtr;
  int imageHeight = *imageHeightPtr;
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

    int lineWidth = scaledWidth * static_cast<int>(useFloats ? sizeof(uint16_t) : sizeof(uint8_t)) * 4;
    int alignment = 64;
    int padding = (alignment - (lineWidth % alignment)) % alignment;
    int imdStride = lineWidth + padding;

    std::vector<uint8_t> newImageData(imdStride * scaledHeight);

    float ratio = std::min(static_cast<float>(scaledHeight) / static_cast<float>(imageHeight),
                           static_cast<float>(scaledWidth) / static_cast<float>(imageWidth));

    if (useFloats) {
      if (ratio < 0.5f) {
        auto kernel = compute1DGaussianKernel(7, (7 - 1) / 6.f);
        coder::convolve1D(reinterpret_cast<uint16_t *>(rgbaData.data()), *stride, imageWidth, imageHeight, kernel, kernel);
      }

      coder::scaleImageFloat16(reinterpret_cast<const uint16_t *>(rgbaData.data()),
                               imageWidth * 4 * (int) sizeof(uint16_t),
                               imageWidth, imageHeight,
                               reinterpret_cast<uint16_t *>(newImageData.data()),
                               imdStride,
                               scaledWidth, scaledHeight,
                               4,
                               sampler
      );
    } else {
      if (ratio < 0.5f) {
        auto kernel = compute1DGaussianKernel(7, (7 - 1) / 6.f);
        coder::convolve1D(reinterpret_cast<uint8_t *>(rgbaData.data()), *stride, imageWidth, imageHeight, kernel, kernel);
      }
      coder::scaleImageU8(reinterpret_cast<const uint8_t *>(rgbaData.data()),
                          (int) imageWidth * 4 * (int) sizeof(uint8_t),
                          imageWidth, imageHeight,
                          reinterpret_cast<uint8_t *>(newImageData.data()),
                          imdStride,
                          scaledWidth, scaledHeight,
                          4, 8,
                          sampler);
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