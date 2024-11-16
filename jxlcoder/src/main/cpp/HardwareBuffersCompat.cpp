/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 16/09/2023
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

#include "HardwareBuffersCompat.h"

#include <mutex>
#include <dlfcn.h>
#include "JniExceptions.h"
#include "jni.h"

std::mutex dlMutex;

bool alreadyHdBuffersLoaded = false;

AHardwareBufferAllocateFunc AHardwareBuffer_allocate_compat = nullptr;
AHardwareBufferIsSupportedFunc AHardwareBuffer_isSupported_compat = nullptr;
AHardwareBufferUnlockFunc AHardwareBuffer_unlock_compat = nullptr;
AHardwareBufferReleaseFunc AHardwareBuffer_release_compat = nullptr;
AHardwareBufferLockFunc AHardwareBuffer_lock_compat = nullptr;
AHardwareBufferToHardwareBufferFunc AHardwareBuffer_toHardwareBuffer_compat = nullptr;
AHardwareBufferDescribeFunc AHardwareBuffer_describe_compat = nullptr;

bool loadAHardwareBuffersAPI() {
  // Bitmap doesn't support wrapping before 29 API
  if (androidOSVersion() < 29) {
    return false;
  }
  std::lock_guard guard(dlMutex);
  if (alreadyHdBuffersLoaded) {
    return AHardwareBuffer_allocate_compat != nullptr
        && AHardwareBuffer_isSupported_compat != nullptr
        && AHardwareBuffer_unlock_compat != nullptr
        && AHardwareBuffer_release_compat != nullptr
        && AHardwareBuffer_lock_compat != nullptr
        && AHardwareBuffer_toHardwareBuffer_compat != nullptr
        && AHardwareBuffer_describe_compat != nullptr;
  }
  alreadyHdBuffersLoaded = true;

  AHardwareBuffer_allocate_compat = (AHardwareBufferAllocateFunc) dlsym(RTLD_DEFAULT,
                                                                        "AHardwareBuffer_allocate");
  if (AHardwareBuffer_allocate_compat == nullptr) {
    return false;
  }

  AHardwareBuffer_isSupported_compat = (AHardwareBufferIsSupportedFunc) dlsym(RTLD_DEFAULT,
                                                                              "AHardwareBuffer_isSupported");
  if (AHardwareBuffer_isSupported_compat == nullptr) {
    return false;
  }

  AHardwareBuffer_unlock_compat = (AHardwareBufferUnlockFunc) dlsym(RTLD_DEFAULT,
                                                                    "AHardwareBuffer_unlock");
  if (AHardwareBuffer_unlock_compat == nullptr) {
    return false;
  }

  AHardwareBuffer_release_compat = (AHardwareBufferReleaseFunc) dlsym(RTLD_DEFAULT,
                                                                      "AHardwareBuffer_release");
  if (AHardwareBuffer_release_compat == nullptr) {
    return false;
  }

  AHardwareBuffer_lock_compat = (AHardwareBufferLockFunc) dlsym(RTLD_DEFAULT,
                                                                "AHardwareBuffer_lock");
  if (AHardwareBuffer_lock_compat == nullptr) {
    return false;
  }

  AHardwareBuffer_toHardwareBuffer_compat = (AHardwareBufferToHardwareBufferFunc) dlsym(RTLD_DEFAULT,
                                                                                        "AHardwareBuffer_toHardwareBuffer");
  if (AHardwareBuffer_toHardwareBuffer_compat == nullptr) {
    return false;
  }

  AHardwareBuffer_describe_compat = (AHardwareBufferDescribeFunc) dlsym(RTLD_DEFAULT,
                                                                        "AHardwareBuffer_describe");
  if (AHardwareBuffer_describe_compat == nullptr) {
    return false;
  }

  return true;
}