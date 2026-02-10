/*
 * MIT License
 *
 * Copyright (c) 2024 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 13/10/2024
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

#include "ColorMatrix.h"
#include "concurrency.hpp"
#include "definitions.h"
#include "Rec2408ToneMapper.h"
#include "LogarithmicToneMapper.h"
#include "ITUR.h"
#include "FilmicToneMapper.h"
#include "AcesToneMapper.h"

#if HAVE_NEON
#include <arm_neon.h>
#endif

static inline void applyMatrix3x3(float *data, uint32_t pixelCount,
                                   float c0, float c1, float c2,
                                   float c3, float c4, float c5,
                                   float c6, float c7, float c8) {
#if HAVE_NEON
  uint32_t x = 0;
  float32x4_t vc0 = vdupq_n_f32(c0), vc1 = vdupq_n_f32(c1), vc2 = vdupq_n_f32(c2);
  float32x4_t vc3 = vdupq_n_f32(c3), vc4 = vdupq_n_f32(c4), vc5 = vdupq_n_f32(c5);
  float32x4_t vc6 = vdupq_n_f32(c6), vc7 = vdupq_n_f32(c7), vc8 = vdupq_n_f32(c8);

  for (; x + 4 <= pixelCount; x += 4) {
    float32x4x3_t rgb = vld3q_f32(data);
    float32x4_t r = rgb.val[0];
    float32x4_t g = rgb.val[1];
    float32x4_t b = rgb.val[2];

    float32x4_t newR = vfmaq_f32(vfmaq_f32(vmulq_f32(r, vc0), g, vc1), b, vc2);
    float32x4_t newG = vfmaq_f32(vfmaq_f32(vmulq_f32(r, vc3), g, vc4), b, vc5);
    float32x4_t newB = vfmaq_f32(vfmaq_f32(vmulq_f32(r, vc6), g, vc7), b, vc8);

    float32x4x3_t out = {{newR, newG, newB}};
    vst3q_f32(data, out);
    data += 12;
  }
  // Scalar tail
  for (; x < pixelCount; ++x) {
    float r = data[0], g = data[1], b = data[2];
    data[0] = r * c0 + g * c1 + b * c2;
    data[1] = r * c3 + g * c4 + b * c5;
    data[2] = r * c6 + g * c7 + b * c8;
    data += 3;
  }
#else
  for (uint32_t x = 0; x < pixelCount; ++x) {
    float r = data[0], g = data[1], b = data[2];
    data[0] = r * c0 + g * c1 + b * c2;
    data[1] = r * c3 + g * c4 + b * c5;
    data[2] = r * c6 + g * c7 + b * c8;
    data += 3;
  }
#endif
}

void applyColorMatrix(uint8_t *inPlace, uint32_t stride, uint32_t width, uint32_t height,
                      const float *matrix, TransferFunction intoLinear, TransferFunction intoGamma,
                      CurveToneMapper toneMapper, ITURColorCoefficients coeffs,
                      float contentBrightness) {
  float c0 = matrix[0];
  float c1 = matrix[1];
  float c2 = matrix[2];
  float c3 = matrix[3];
  float c4 = matrix[4];
  float c5 = matrix[5];
  float c6 = matrix[6];
  float c7 = matrix[7];
  float c8 = matrix[8];

  float linearizeMap[256] = {0};
  for (uint32_t j = 0; j < 256; ++j) {
    linearizeMap[j] = toLinear(static_cast<float>(j) * (1.f / 255.f), intoLinear);
  }

  uint8_t gammaMap[2049] = {0};
  for (uint32_t j = 0; j < 2049; ++j) {
    gammaMap[j] = static_cast<uint8_t >(std::clamp(
        std::roundf(toGamma(static_cast<float>(j) * (1.f / 2048.f), intoGamma) * 255.f),
        0.f,
        255.f));
  }

  concurrency::parallel_for(6, height, [&](uint32_t y) {
    aligned_float_vector rowVector(width * 3);
    uint8_t *sourceRow = inPlace + y * stride;

    float *rowData = rowVector.data();

    for (uint32_t x = 0; x < width; ++x) {
      rowData[0] = linearizeMap[sourceRow[0]];
      rowData[1] = linearizeMap[sourceRow[1]];
      rowData[2] = linearizeMap[sourceRow[2]];
      rowData += 3;
      sourceRow += 4;
    }

    float mCoeffs[3] = {coeffs.kr, coeffs.kg, coeffs.kb};
    if (toneMapper == CurveToneMapper::REC2408 || toneMapper == CurveToneMapper::REC2408_PERCEPTUAL) {
      Rec2408ToneMapper mToneMapper(contentBrightness, 250.f, 203.f, mCoeffs, toneMapper == CurveToneMapper::REC2408_PERCEPTUAL);
      mToneMapper.transferTone(rowVector.data(), width);
    } else if (toneMapper == CurveToneMapper::LOGARITHMIC) {
      LogarithmicToneMapper mToneMapper(mCoeffs);
      mToneMapper.transferTone(rowVector.data(), width);
    } else if (toneMapper == CurveToneMapper::FILMIC) {
      FilmicToneMapper filmicToneMapper;
      filmicToneMapper.transferTone(rowVector.data(), width);
    } else if (toneMapper == CurveToneMapper::ACES) {
      AcesToneMapper acesToneMapper;
      acesToneMapper.transferTone(rowVector.data(), width);
    }

    applyMatrix3x3(rowVector.data(), width, c0, c1, c2, c3, c4, c5, c6, c7, c8);

    float *iter = rowVector.data();
    sourceRow = inPlace + y * stride;

    for (uint32_t x = 0; x < width; ++x) {
      uint16_t scaledValue0 = std::min(
          static_cast<uint16_t >(std::clamp(iter[0], 0.f, 1.0f) * 2048.f),
          static_cast<uint16_t>(2048));
      uint16_t scaledValue1 = std::min(
          static_cast<uint16_t >(std::clamp(iter[1], 0.f, 1.0f) * 2048.f),
          static_cast<uint16_t>(2048));
      uint16_t scaledValue2 = std::min(
          static_cast<uint16_t >(std::clamp(iter[2], 0.f, 1.0f) * 2048.f),
          static_cast<uint16_t>(2048));
      sourceRow[0] = gammaMap[scaledValue0];
      sourceRow[1] = gammaMap[scaledValue1];
      sourceRow[2] = gammaMap[scaledValue2];
      sourceRow += 4;
      iter += 3;
    }
  });
}

void applyColorMatrix16Bit(uint16_t *inPlace,
                           uint32_t stride,
                           uint32_t width,
                           uint32_t height,
                           uint8_t bitDepth,
                           const float *matrix,
                           TransferFunction intoLinear,
                           TransferFunction intoGamma,
                           CurveToneMapper toneMapper,
                           ITURColorCoefficients coeffs,
                           float contentBrightness) {

  float c0 = matrix[0];
  float c1 = matrix[1];
  float c2 = matrix[2];
  float c3 = matrix[3];
  float c4 = matrix[4];
  float c5 = matrix[5];
  float c6 = matrix[6];
  float c7 = matrix[7];
  float c8 = matrix[8];

  uint32_t maxColors = 1 << bitDepth;
  uint16_t iCutOff = maxColors - 1;
  float cutOffColors = static_cast<float>(maxColors) - 1;
  float scaleCutOff = 1.f / cutOffColors;

  aligned_float_vector linearizeMap(maxColors);
  for (uint32_t j = 0; j < maxColors; ++j) {
    linearizeMap[j] = toLinear(static_cast<float>(j) * scaleCutOff, intoLinear);
  }

  aligned_uint16_vector gammaMap(maxColors);
  for (uint32_t j = 0; j < maxColors; ++j) {
    gammaMap[j] = static_cast<uint16_t >(std::clamp(
        std::roundf(toGamma(static_cast<float>(j) * scaleCutOff, intoGamma) * cutOffColors),
        0.f,
        cutOffColors));
  }

  concurrency::parallel_for(6, height, [&](uint32_t y) {
    aligned_float_vector rowVector(width * 3);
    auto sourceRow = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t * >(inPlace) + y * stride);

    float *rowData = rowVector.data();

    for (uint32_t x = 0; x < width; ++x) {
      rowData[0] = linearizeMap[std::min(sourceRow[0], iCutOff)];
      rowData[1] = linearizeMap[std::min(sourceRow[1], iCutOff)];
      rowData[2] = linearizeMap[std::min(sourceRow[2], iCutOff)];
      rowData += 3;
      sourceRow += 4;
    }

    float mCoeffs[3] = {coeffs.kr, coeffs.kg, coeffs.kb};
    if (toneMapper == CurveToneMapper::REC2408 || toneMapper == CurveToneMapper::REC2408_PERCEPTUAL) {
      Rec2408ToneMapper mToneMapper(contentBrightness, 250.f, 203.f, mCoeffs, toneMapper == CurveToneMapper::REC2408_PERCEPTUAL);
      mToneMapper.transferTone(rowVector.data(), width);
    } else if (toneMapper == CurveToneMapper::LOGARITHMIC) {
      LogarithmicToneMapper mToneMapper(mCoeffs);
      mToneMapper.transferTone(rowVector.data(), width);
    } else if (toneMapper == CurveToneMapper::FILMIC) {
      FilmicToneMapper filmicToneMapper;
      filmicToneMapper.transferTone(rowVector.data(), width);
    } else if (toneMapper == CurveToneMapper::ACES) {
      AcesToneMapper acesToneMapper;
      acesToneMapper.transferTone(rowVector.data(), width);
    }

    applyMatrix3x3(rowVector.data(), width, c0, c1, c2, c3, c4, c5, c6, c7, c8);

    float *iter = rowVector.data();
    sourceRow = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t * >(inPlace) + y * stride);

    for (uint32_t x = 0; x < width; ++x) {
      uint16_t scaledValue0 = std::min(
          static_cast<uint16_t >(std::clamp(iter[0], 0.f, 1.0f) * cutOffColors),
          static_cast<uint16_t>(cutOffColors));
      uint16_t scaledValue1 = std::min(
          static_cast<uint16_t >(std::clamp(iter[1], 0.f, 1.0f) * cutOffColors),
          static_cast<uint16_t>(cutOffColors));
      uint16_t scaledValue2 = std::min(
          static_cast<uint16_t >(std::clamp(iter[2], 0.f, 1.0f) * cutOffColors),
          static_cast<uint16_t>(cutOffColors));

      sourceRow[0] = gammaMap[scaledValue0];
      sourceRow[1] = gammaMap[scaledValue1];
      sourceRow[2] = gammaMap[scaledValue2];
      sourceRow += 4;
      iter += 3;
    }
  });
}