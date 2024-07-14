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

extern "C" {

void weave_scale_f16(const uint16_t *src,
                     uint32_t src_stride,
                     uint32_t width,
                     uint32_t height,
                     const uint16_t *dst,
                     uint32_t dst_stride,
                     uint32_t new_width,
                     uint32_t new_height,
                     ScalingFunction scaling_function);

void weave_scale_u8(const uint8_t *src,
                    uint32_t src_stride,
                    uint32_t width,
                    uint32_t height,
                    const uint8_t *dst,
                    uint32_t dst_stride,
                    uint32_t new_width,
                    uint32_t new_height,
                    ScalingFunction scaling_function);

} // extern "C"
