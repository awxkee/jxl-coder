#pragma once

#include <jni.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include "weaver.h"

/// Move-only C++ owner for the opaque Box<JxlAnimationCoordinator> allocated by Rust.
class RustJxlAnimationCoordinator final {
 public:
  WeaveScaleMode scaleMode = WeaveScaleMode::ScaleToFit;
  WeaverPreferredColorConfig preferredConfig = WeaverPreferredColorConfig::Default;

  class Frame final {
   public:
    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;

    Frame(Frame &&other) noexcept: frame_(other.release()) {}

    Frame &operator=(Frame &&other) noexcept {
      if (this != &other) {
        reset();
        frame_ = other.release();
      }
      return *this;
    }

    ~Frame() { reset(); }

    [[nodiscard]] const uint8_t *pixels() const noexcept { return frame_.pixels.data; }
    [[nodiscard]] size_t pixels_size() const noexcept { return frame_.pixels.length; }
    [[nodiscard]] const uint8_t *icc() const noexcept { return frame_.icc.data; }
    [[nodiscard]] size_t icc_size() const noexcept { return frame_.icc.length; }
    [[nodiscard]] uint32_t width() const noexcept { return frame_.width; }
    [[nodiscard]] uint32_t height() const noexcept { return frame_.height; }
    [[nodiscard]] uint32_t stride() const noexcept { return frame_.stride; }
    [[nodiscard]] uint32_t duration_ms() const noexcept { return frame_.duration_ms; }
    [[nodiscard]] bool has_alpha() const noexcept { return frame_.has_alpha; }
    [[nodiscard]] bool alpha_premultiplied() const noexcept {
      return frame_.alpha_premultiplied;
    }

   private:
    friend class RustJxlAnimationCoordinator;

    explicit Frame(JxlAnimationFrame frame) noexcept: frame_(frame) {}

    static JxlAnimationFrame empty() noexcept {
      return {{nullptr, 0, 0}, {nullptr, 0, 0}, 0, 0, 0, 0, false, false};
    }

    JxlAnimationFrame release() noexcept {
      JxlAnimationFrame value = frame_;
      frame_ = empty();
      return value;
    }

    void reset() noexcept {
      jxl_animation_frame_release(frame_);
      frame_ = empty();
    }

    JxlAnimationFrame frame_ = empty();
  };

  RustJxlAnimationCoordinator(const RustJxlAnimationCoordinator &) = delete;
  RustJxlAnimationCoordinator &operator=(const RustJxlAnimationCoordinator &) = delete;

  RustJxlAnimationCoordinator(RustJxlAnimationCoordinator &&other) noexcept
      : coordinator_(std::exchange(other.coordinator_, nullptr)) {}

  RustJxlAnimationCoordinator &operator=(RustJxlAnimationCoordinator &&other) noexcept {
    if (this != &other) {
      reset();
      coordinator_ = std::exchange(other.coordinator_, nullptr);
    }
    return *this;
  }

  ~RustJxlAnimationCoordinator() { reset(); }

  static RustJxlAnimationCoordinator *create(const uint8_t *data, size_t size) {
    JxlAnimationCreateResult result = jxl_animation_coordinator_create(data, size);
    if (result.status != JxlAnimationStatus::Ok) {
      throw std::runtime_error(take_error(result.error, "Cannot create JXL animation decoder"));
    }
    jxl_animation_buffer_release(result.error);
    return new RustJxlAnimationCoordinator(result.coordinator);
  }

  [[nodiscard]] JxlAnimationInfo info() const {
    JxlAnimationInfo value{};
    const JxlAnimationStatus status =
        jxl_animation_coordinator_get_info(require_open(), &value);
    throw_on_status(status, "Cannot read JXL animation info");
    return value;
  }

  [[nodiscard]] uint32_t frame_duration(uint32_t index) const {
    uint32_t duration = 0;
    const JxlAnimationStatus status =
        jxl_animation_coordinator_get_frame_duration(require_open(), index, &duration);
    throw_on_status(status, "Cannot read JXL frame duration");
    return duration;
  }

  /// Returns a ready-to-use android.graphics.Bitmap local reference.
  /// Rust performs frame decoding, ICC conversion, scaling, and Bitmap creation.
  /// Decode failures are reported as pending Java RuntimeExceptions.
  [[nodiscard]] jobject getFrame(
      JNIEnv *env,
      uint32_t index,
      int32_t scaled_width = 0,
      int32_t scaled_height = 0) const {
    if (!env) {
      throw std::invalid_argument("JNIEnv is null");
    }
    return jxl_animation_coordinator_get_frame_bitmap(
        env, require_open(), index,
        scaled_width, scaled_height,
        this->scaleMode, this->preferredConfig);
  }

  [[nodiscard]] Frame decode_frame(uint32_t index) const {
    JxlAnimationFrameResult result =
        jxl_animation_coordinator_decode_frame(require_open(), index);
    if (result.status != JxlAnimationStatus::Ok) {
      throw std::runtime_error(take_error(result.error, "Cannot decode JXL animation frame"));
    }
    jxl_animation_buffer_release(result.error);
    return Frame(result.frame);
  }

 private:
  explicit RustJxlAnimationCoordinator(JxlAnimationCoordinator *coordinator) noexcept
      : coordinator_(coordinator) {}

  static std::string take_error(JxlAnimationBuffer error, const char *fallback) {
    std::string message = error.data && error.length
                          ? std::string(reinterpret_cast<const char *>(error.data), error.length)
                          : std::string(fallback);
    jxl_animation_buffer_release(error);
    return message;
  }

  static void throw_on_status(JxlAnimationStatus status, const char *message) {
    if (status != JxlAnimationStatus::Ok) {
      throw std::runtime_error(std::string(message) + " (status " +
          std::to_string(static_cast<int>(status)) + ")");
    }
  }

  [[nodiscard]] const JxlAnimationCoordinator *require_open() const {
    if (!coordinator_) {
      throw std::logic_error("JXL animation coordinator is closed");
    }
    return coordinator_;
  }

  void reset() noexcept {
    if (coordinator_) {
      jxl_animation_coordinator_destroy(coordinator_);
      coordinator_ = nullptr;
    }
  }

  JxlAnimationCoordinator *coordinator_ = nullptr;
};
