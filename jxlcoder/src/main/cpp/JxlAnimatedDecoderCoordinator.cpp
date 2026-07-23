/*
 * MIT License
 *
 * Copyright (c) 2023-2026 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 08/01/2024
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

#include "JxlAnimatedDecoderCoordinator.h"
#include "Support.h"
#include "JniExceptions.h"
#include <jni.h>
#include "android/bitmap.h"
#include "jxl_animation.hpp"

using namespace std;

extern "C"
JNIEXPORT jlong JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_createCoordinator(JNIEnv *env, jobject thiz,
                                                            jobject byteBuffer,
                                                            jint javaPreferredColorConfig,
                                                            jint javaScaleMode) {
  try {
    auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
    int length = (int) env->GetDirectBufferCapacity(byteBuffer);
    if (!bufferAddress || length <= 0) {
      std::string errorString = "Only direct byte buffers are supported";
      throwException(env, errorString);
      return 0;
    }
    vector<uint8_t> srcBuffer(length);
    copy(bufferAddress, bufferAddress + length, srcBuffer.begin());
    auto weaveScaleMode = javaScaleModeToRust(javaScaleMode);
    auto weaveColorConfig = javeConfigToRust(javaPreferredColorConfig);
    auto coordinator = RustJxlAnimationCoordinator::create(srcBuffer.data(), length);
    coordinator->scaleMode = weaveScaleMode;
    coordinator->preferredConfig = weaveColorConfig;
    return reinterpret_cast<jlong >(coordinator);
  } catch (std::runtime_error &err) {
    std::string errorString = err.what();
    throwException(env, errorString);
    return 0;
  } catch (std::bad_alloc &err) {
    std::string errorString = "OOM: " + string(err.what());
    throwException(env, errorString);
    return 0;
  }
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_createCoordinatorByteArray(JNIEnv *env, jobject thiz,
                                                                     jbyteArray byteArray,
                                                                     jint javaPreferredColorConfig,
                                                                     jint javaScaleMode) {
  try {
    auto length = env->GetArrayLength(byteArray);
    vector<uint8_t> srcBuffer(length);
    env->GetByteArrayRegion(byteArray, 0, length,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    auto weaveScaleMode = javaScaleModeToRust(javaScaleMode);
    auto weaveColorConfig = javeConfigToRust(javaPreferredColorConfig);
    auto coordinator = RustJxlAnimationCoordinator::create(srcBuffer.data(), length);
    coordinator->scaleMode = weaveScaleMode;
    coordinator->preferredConfig = weaveColorConfig;
    return reinterpret_cast<jlong >(coordinator);
  } catch (std::bad_alloc &err) {
    std::string errorString = "OOM: " + string(err.what());
    throwException(env, errorString);
    return 0;
  } catch (std::runtime_error &err) {
    std::string errorString = err.what();
    throwException(env, errorString);
    return 0;
  }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_closeAndReleaseAnimatedImage(JNIEnv *env, jobject thiz,
                                                                       jlong coordinatorPtr) {
  auto coordinator = reinterpret_cast<RustJxlAnimationCoordinator *>(coordinatorPtr);
  delete coordinator;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getNumberOfFrames(JNIEnv *env, jobject thiz,
                                                            jlong coordinatorPtr) {
  auto coordinator = reinterpret_cast<RustJxlAnimationCoordinator *>(coordinatorPtr);
  return static_cast<jint>(coordinator->info().frame_count);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getFrameDurationImpl(JNIEnv *env, jobject thiz,
                                                               jlong coordinatorPtr, jint frame) {
  auto coordinator = reinterpret_cast<RustJxlAnimationCoordinator *>(coordinatorPtr);
  return static_cast<jint>(coordinator->frame_duration(frame));
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getLoopsCount(JNIEnv *env, jobject thiz,
                                                        jlong coordinatorPtr) {
  auto coordinator = reinterpret_cast<RustJxlAnimationCoordinator *>(coordinatorPtr);
  return coordinator->info().loop_count;
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getFrameImpl(JNIEnv *env, jobject thiz,
                                                       jlong coordinatorPtr, jint frameIndex,
                                                       jint scaleWidth, jint scaleHeight) {
  try {
    auto coordinator = reinterpret_cast<RustJxlAnimationCoordinator *>(coordinatorPtr);

    return coordinator->getFrame(env, frameIndex, scaleWidth, scaleWidth);
  } catch (std::bad_alloc &err) {
    std::string errorString = "OOM: " + string(err.what());
    throwException(env, errorString);
    return nullptr;
  } catch (std::runtime_error &err) {
    std::string errorString = err.what();
    throwException(env, errorString);
    return nullptr;
  }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getHeightImpl(JNIEnv *env, jobject thiz,
                                                        jlong coordinatorPtr) {
  auto coordinator = reinterpret_cast<RustJxlAnimationCoordinator *>(coordinatorPtr);
  return static_cast<jint>(coordinator->info().height);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getWidthImpl(JNIEnv *env, jobject thiz,
                                                       jlong coordinatorPtr) {
  auto coordinator = reinterpret_cast<RustJxlAnimationCoordinator *>(coordinatorPtr);
  return static_cast<jint>(coordinator->info().width);
}