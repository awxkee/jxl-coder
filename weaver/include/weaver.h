#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

/// JPEG XL may be represented as either a bare codestream or a box container.
enum class ImageContainer {
  Unknown = 0,
  JxlCodestream = 1,
  JxlContainer = 2,
};

enum class JixelEncodingSpeed {
  Slow,
  Fast,
};

enum class JxlAnimationStatus {
  Ok = 0,
  NullPointer = 1,
  InvalidJxl = 2,
  TruncatedInput = 3,
  FrameOutOfRange = 4,
  AllocationFailed = 5,
  DecodeFailed = 6,
  Panic = 7,
};

enum class WeaveScaleMode {
  JustResize,
  ScaleToFill,
  ScaleToFit,
};

enum class WeaverPreferredColorConfig {
  Default,
  Rgba8888 = 2,
  RgbaF16 = 3,
  Rgb565 = 4,
  Rgba1010102 = 5,
  Hardware = 6,
};

enum class FfiTrc {
  /// For future use by ITU-T | ISO/IEC
  Reserved,
  /// Rec. ITU-R BT.709-6<br />
  /// Rec. ITU-R BT.1361-0 conventional colour gamut system (historical)<br />
  /// (functionally the same as the values 6, 14 and 15)    <br />
  Bt709 = 1,
  /// Image characteristics are unknown or are determined by the application.<br />
  Unspecified = 2,
  /// Rec. ITU-R BT.470-6 System M (historical)<br />
  /// United States National Television System Committee 1953 Recommendation for transmission standards for color television<br />
  /// United States Federal Communications Commission (2003) Title 47 Code of Federal Regulations 73.682 (a) (20)<br />
  /// Rec. ITU-R BT.1700-0 625 PAL and 625 SECAM<br />
  Bt470M = 4,
  /// Rec. ITU-R BT.470-6 System B, G (historical)<br />
  Bt470Bg = 5,
  /// Rec. ITU-R BT.601-7 525 or 625<br />
  /// Rec. ITU-R BT.1358-1 525 or 625 (historical)<br />
  /// Rec. ITU-R BT.1700-0 NTSC SMPTE 170M (2004)<br />
  /// (functionally the same as the values 1, 14 and 15)<br />
  Bt601 = 6,
  /// SMPTE 240M (1999) (historical)<br />
  Smpte240 = 7,
  /// Linear transfer characteristics<br />
  Linear = 8,
  /// Logarithmic transfer characteristic (100:1 range)<br />
  Log100 = 9,
  /// Logarithmic transfer characteristic (100 * Sqrt( 10 ) : 1 range)<br />
  Log100sqrt10 = 10,
  /// IEC 61966-2-4<br />
  Iec61966 = 11,
  /// Rec. ITU-R BT.1361-0 extended colour gamut system (historical)<br />
  Bt1361 = 12,
  /// IEC 61966-2-1 sRGB or sYCC<br />
  Srgb = 13,
  /// Rec. ITU-R BT.2020-2 (10-bit system)<br />
  /// (functionally the same as the values 1, 6 and 15)<br />
  Bt202010bit = 14,
  /// Rec. ITU-R BT.2020-2 (12-bit system)<br />
  /// (functionally the same as the values 1, 6 and 14)<br />
  Bt202012bit = 15,
  /// SMPTE ST 2084 for 10-, 12-, 14- and 16-bitsystems<br />
  /// Rec. ITU-R BT.2100-0 perceptual quantization (PQ) system<br />
  Smpte2084 = 16,
  /// SMPTE ST 428-1<br />
  Smpte428 = 17,
  /// ARIB STD-B67<br />
  /// Rec. ITU-R BT.2100-0 hybrid log- gamma (HLG) system<br />
  Hlg = 18,
};

enum class ToneMapping {
  Skip,
  Rec2408,
};

/// Opaque Rust-owned decoder state. C++ must only use pointers to this type.
struct JxlAnimationCoordinator;

struct FfiProfileData {
  uint8_t *data;
  uintptr_t size;
  uintptr_t capacity;
};

struct JxlAnimationBuffer {
  uint8_t *data;
  uintptr_t length;
  uintptr_t capacity;
};

struct JxlAnimationCreateResult {
  JxlAnimationCoordinator *coordinator;
  JxlAnimationStatus status;
  /// UTF-8 bytes; not NUL-terminated. Release with `jxl_animation_buffer_release`.
  JxlAnimationBuffer error;
};

struct JxlAnimationInfo {
  uint32_t width;
  uint32_t height;
  uint32_t source_bit_depth;
  uint32_t frame_count;
  /// JPEG XL semantics: 0 means infinite. Non-animated images report -1.
  int32_t loop_count;
  bool has_alpha;
};

struct JxlAnimationFrame {
  /// Tightly packed RGBA8 pixels with `stride == width * 4`.
  JxlAnimationBuffer pixels;
  /// ICC profile describing `pixels`; may be empty.
  JxlAnimationBuffer icc;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t duration_ms;
  bool has_alpha;
  /// jxl-rs currently emits straight alpha for this API.
  bool alpha_premultiplied;
};

struct JxlAnimationFrameResult {
  JxlAnimationFrame frame;
  JxlAnimationStatus status;
  /// UTF-8 bytes; not NUL-terminated. Release with `jxl_animation_buffer_release`.
  JxlAnimationBuffer error;
};

struct JxlInfo {
  bool supported_image;
  uint32_t width;
  uint32_t height;
  uint32_t bit_depth;
};

struct ScalingResult {
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

/// Returns true for both bare JPEG XL codestreams and JXL containers.
bool is_jxl_image(const uint8_t *data, uintptr_t len);

/// Returns true only for a bare JPEG XL codestream.
bool is_jxl_codestream(const uint8_t *data, uintptr_t len);

/// Returns true only for a JPEG XL box container.
bool is_jxl_container(const uint8_t *data, uintptr_t len);

ImageContainer container_recognisance(const uint8_t *data, uintptr_t len);

void weave_cvt_rgba8_to_rgba_f16(const uint8_t *rgba8,
                                 uint32_t rgba8_stride,
                                 uint16_t *rgba_f16,
                                 uint32_t rgba_f16_stride,
                                 uint32_t width,
                                 uint32_t height);

void weave_premultiply_rgba_f16(uint16_t *rgba_f16,
                                uint32_t rgba_f16_stride,
                                uint32_t width,
                                uint32_t height);

void weave_cvt_rgba8_to_ar30(const uint8_t *rgba8,
                             uint32_t rgba8_stride,
                             uint8_t *ar30,
                             uint32_t ar30_stride,
                             uint32_t width,
                             uint32_t height);

void weave_cvt_rgba16_to_ar30(const uint16_t *rgba16,
                              uint32_t rgba16_stride,
                              uintptr_t bit_depth,
                              uint8_t *ar30,
                              uint32_t ar30_stride,
                              uint32_t width,
                              uint32_t height);

void weave_cvt_rgba16_to_rgba_f16(const uint16_t *rgba16,
                                  uint32_t rgba16_stride,
                                  uintptr_t bit_depth,
                                  uint16_t *rgba_f16,
                                  uint32_t rgba_f16_stride,
                                  uint32_t width,
                                  uint32_t height);

void apply_icc_rgba8(const uint8_t *src_image,
                     uint32_t src_stride,
                     uint8_t *dst_image,
                     uint32_t dst_stride,
                     uint32_t width,
                     uint32_t height,
                     const uint8_t *icc_profile,
                     uint32_t icc_profile_stride);

void apply_icc_rgba16(const uint16_t *src_image,
                      uint32_t src_stride,
                      uint16_t *dst_image,
                      uint32_t dst_stride,
                      uint32_t bit_depth,
                      uint32_t width,
                      uint32_t height,
                      const uint8_t *icc_profile,
                      uint32_t icc_profile_stride);

void free_profile(FfiProfileData wrapper);

FfiProfileData new_dci_p3_profile();

FfiProfileData new_adobe_rgb_profile();

jbyteArray encode_jixel_file(JNIEnv *env,
                             jobject image,
                             jobject exif,
                             int32_t color_space,
                             int32_t quality,
                             bool lossless,
                             JixelEncodingSpeed speed);

JxlAnimationCreateResult jxl_animation_coordinator_create(const uint8_t *data, uintptr_t length);

void jxl_animation_coordinator_destroy(JxlAnimationCoordinator *coordinator);

JxlAnimationStatus jxl_animation_coordinator_get_info(const JxlAnimationCoordinator *coordinator,
                                                      JxlAnimationInfo *info);

JxlAnimationStatus jxl_animation_coordinator_get_frame_duration(const JxlAnimationCoordinator *coordinator,
                                                                uint32_t index,
                                                                uint32_t *duration_ms);

JxlAnimationFrameResult jxl_animation_coordinator_decode_frame(const JxlAnimationCoordinator *coordinator,
                                                               uint32_t index);

void jxl_animation_buffer_release(JxlAnimationBuffer buffer);

void jxl_animation_frame_release(JxlAnimationFrame frame);

/// Decodes an animation frame and returns a ready-to-use `android.graphics.Bitmap`.
///
/// A Java `RuntimeException` is pending and null is returned if decoding or bitmap
/// creation fails. The returned JNI local reference is owned by the calling frame.
jobject jxl_animation_coordinator_get_frame_bitmap(JNIEnv *env,
                                                   const JxlAnimationCoordinator *coordinator,
                                                   uint32_t index,
                                                   int32_t scaled_width,
                                                   int32_t scaled_height,
                                                   WeaveScaleMode scale_mode,
                                                   WeaverPreferredColorConfig preferred_color_config);

jobject decode_jxl_file(JNIEnv *env,
                        const uint8_t *data,
                        uintptr_t length,
                        int32_t scaled_width,
                        int32_t scaled_height,
                        WeaveScaleMode scale_mode,
                        WeaverPreferredColorConfig preferred_color_config);

JxlInfo read_jxl_file_info(const uint8_t *data, uintptr_t length);

void weave_scaling_result_free(ScalingResult result);

void weave_scaling_result16_free(ScalingResultU16 result);

ScalingResult weave_scale_u8(const uint8_t *src,
                             uint32_t src_stride,
                             uint32_t width,
                             uint32_t height,
                             int32_t new_width,
                             int32_t new_height,
                             bool premultiply_alpha,
                             WeaveScaleMode scale_mode);

ScalingResultU16 weave_scale_u16(const uint16_t *src,
                                 uintptr_t src_stride,
                                 uint32_t width,
                                 uint32_t height,
                                 int32_t new_width,
                                 int32_t new_height,
                                 uintptr_t bit_depth,
                                 bool premultiply_alpha,
                                 WeaveScaleMode scale_mode);

void weave_scale_f16(const uint16_t *src,
                     uintptr_t src_stride,
                     uint32_t width,
                     uint32_t height,
                     uint16_t *dst,
                     uint32_t new_width,
                     uint32_t new_height,
                     uint32_t method,
                     bool premultiply_alpha);

void apply_tone_mapping_rgba8(uint8_t *image,
                              uint32_t stride,
                              uint32_t width,
                              uint32_t height,
                              const float *primaries,
                              const float *white_point,
                              FfiTrc trc,
                              ToneMapping mapping,
                              float brightness);

void apply_tone_mapping_rgba16(uint16_t *image,
                               uint32_t stride,
                               uint32_t bit_depth,
                               uint32_t width,
                               uint32_t height,
                               const float *primaries,
                               const float *white_point,
                               FfiTrc trc,
                               ToneMapping mapping,
                               float brightness);

jbyteArray encode_jixel_file(JNIEnv *env,
                             jobject image,
                             jobject exif,
                             int32_t color_space,
                             int32_t quality,
                             bool lossless,
                             JixelEncodingSpeed speed);

}  // extern "C"
