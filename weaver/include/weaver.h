#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

enum class ScalingFunction {
  Bilinear = 1,
  Nearest = 2,
  Cubic = 3,
  Mitchell = 4,
  Lanczos = 5,
  CatmullRom = 6,
  Hermite = 7,
  BSpline = 8,
  Bicubic = 9,
  Box = 10,
};

enum class WeaveScaleMode {
  JustResize,
  ScaleToFill,
  ScaleToFit,
};

struct ScalingResultU8 {
  uint8_t *data;
  uintptr_t width;
  uintptr_t height;
  uintptr_t stride;
  uintptr_t length;
  uintptr_t capacity;
};

struct ScalingResultU16 {
  uint16_t *data;
  uintptr_t width;
  uintptr_t height;
  uintptr_t stride;
  uintptr_t length;
  uintptr_t capacity;
};

extern "C" {

void weave_scaling_result_free(ScalingResultU8 result);

void weave_scaling_result16_free(ScalingResultU16 result);

ScalingResultU8 weave_scale_u8(const uint8_t *src,
                               uint32_t src_stride,
                               uint32_t width,
                               uint32_t height,
                               int32_t new_width,
                               int32_t new_height,
                               ScalingFunction scaling_function,
                               bool premultiply_alpha,
                               WeaveScaleMode scale_mode);

ScalingResultU16 weave_scale_u16(const uint16_t *src,
                                 uintptr_t src_stride,
                                 uint32_t width,
                                 uint32_t height,
                                 int32_t new_width,
                                 int32_t new_height,
                                 uintptr_t bit_depth,
                                 ScalingFunction scaling_function,
                                 bool premultiply_alpha,
                                 WeaveScaleMode scale_mode);

}  // extern "C"
