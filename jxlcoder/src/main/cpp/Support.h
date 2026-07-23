/*
 * MIT License
 *
 * Copyright (c) 2023-2026 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 15/09/2023
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

#ifndef AVIF_SUPPORT_H
#define AVIF_SUPPORT_H

#include <jni.h>
#include "weaver.h"

enum PreferredColorConfig {
  Default = 1,
  Rgba_8888 = 2,
  Rgba_F16 = 3,
  Rgb_565 = 4,
  Rgba_1010102 = 5,
  Hardware = 6
};

inline WeaverPreferredColorConfig javeConfigToRust(jint javaPreferredColorConfig) {
  if (javaPreferredColorConfig == 1) {
    return WeaverPreferredColorConfig::Default;
  } else if (javaPreferredColorConfig == 2) {
    return WeaverPreferredColorConfig::Rgba8888;
  } else if (javaPreferredColorConfig == 3) {
    return WeaverPreferredColorConfig::RgbaF16;
  } else if (javaPreferredColorConfig == 4) {
    return WeaverPreferredColorConfig::Rgb565;
  } else if (javaPreferredColorConfig == 5) {
    return WeaverPreferredColorConfig::Rgba1010102;
  } else if (javaPreferredColorConfig == 6) {
    return WeaverPreferredColorConfig::Hardware;
  }
  return WeaverPreferredColorConfig::Rgba8888;
}

inline WeaveScaleMode javaScaleModeToRust(jint javaScaleMode) {
  if (javaScaleMode == 1) {
    return WeaveScaleMode::ScaleToFit;
  } else if (javaScaleMode == 2) {
    return WeaveScaleMode::ScaleToFill;
  }
  return WeaveScaleMode::JustResize;
}

#endif //AVIF_SUPPORT_H
