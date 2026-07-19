/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 04/09/2023
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

#include <jni.h>
#include <vector>
#include "JniExceptions.h"
#include "android/bitmap.h"
#include "Support.h"
#include "weaver.h"

extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_decodeSampledImpl(JNIEnv *env, jobject thiz,
                                                    jbyteArray byte_array, jint scaledWidth,
                                                    jint scaledHeight,
                                                    jint javaPreferredColorConfig,
                                                    jint javaScaleMode) {
  try {
    auto totalLength = env->GetArrayLength(byte_array);
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(byte_array, 0, totalLength,
                            reinterpret_cast<jbyte *>(srcBuffer.data()));
    return decode_jxl_file(env, srcBuffer.data(), totalLength,
                           scaledWidth, scaledHeight,
                           javaScaleModeToRust(javaScaleMode),
                           javeConfigToRust(javaPreferredColorConfig));
  } catch (std::bad_alloc &err) {
    std::string errorString = "Not enough memory to decode this image";
    throwException(env, errorString);
    return nullptr;
  } catch (std::runtime_error &err) {
    std::string w1 = err.what();
    std::string errorString = "Error while decoding: " + w1;
    throwException(env, errorString);
    return nullptr;
  }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_decodeByteBufferSampledImpl(JNIEnv *env, jobject thiz,
                                                              jobject byteBuffer, jint scaledWidth,
                                                              jint scaledHeight,
                                                              jint preferredColorConfig,
                                                              jint scaleMode) {
  try {
    auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
    int length = (int) env->GetDirectBufferCapacity(byteBuffer);
    if (!bufferAddress || length <= 0) {
      std::string errorString = "Only direct byte buffers are supported";
      throwException(env, errorString);
      return nullptr;
    }
    return decode_jxl_file(env, bufferAddress, length,
                           scaledWidth, scaledHeight,
                           javaScaleModeToRust(scaleMode),
                           javeConfigToRust(preferredColorConfig));
  } catch (std::bad_alloc &err) {
    std::string errorString = "Not enough memory to decode this image";
    throwException(env, errorString);
    return nullptr;
  } catch (std::runtime_error &err) {
    std::string w1 = err.what();
    std::string errorString = "Error while decoding: " + w1;
    throwException(env, errorString);
    return nullptr;
  }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_getSizeImpl(JNIEnv *env, jobject thiz, jbyteArray byte_array) {
  auto totalLength = env->GetArrayLength(byte_array);
  std::shared_ptr<void> srcBuffer(static_cast<char *>(malloc(totalLength)),
                                  [](void *b) { free(b); });
  env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.get()));
  auto info = read_jxl_file_info(reinterpret_cast<const uint8_t *>(srcBuffer.get()), totalLength);
  if (info.width == 0 || info.height == 0) {
    return nullptr;
  }
  jclass sizeClass = env->FindClass("android/util/Size");
  jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
  auto sizeObject = env->NewObject(sizeClass, methodID,
                                   static_cast<jint >(info.width),
                                   static_cast<jint>(info.height));
  return sizeObject;
}