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
#include <string>
#include <vector>
#include <cinttypes>
#include "android/bitmap.h"
#include <android/log.h>
#include "JniExceptions.h"
#include <android/data_space.h>
#include "weaver.h"

using namespace std;

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_encodeImpl(JNIEnv *env, jobject thiz,
                                             jobject bitmap,
                                             jobject exifData,
                                             jint javaCompressionOption,
                                             jint effort, jstring bitmapColorProfile,
                                             jint dataSpace, jint jQuality) {
  try {

    if (jQuality < 0 || jQuality > 100) {
      std::string exc = "Quality must be in 0...100";
      throwException(env, exc);
      return static_cast<jbyteArray>(nullptr);
    }

    JixelEncodingSpeed encodingSpeed = JixelEncodingSpeed::Fast;
    if (effort == 2) {
      encodingSpeed = JixelEncodingSpeed::Slow;
    }

    auto lossless = false;
    if (javaCompressionOption == 1) {
      lossless = true;
    }

    auto compressedData = encode_jixel_file(env, bitmap, exifData, dataSpace, jQuality, lossless, encodingSpeed);
    return compressedData;
  } catch (std::bad_alloc &err) {
    std::string errorString = "Not enough memory to encode this image";
    throwException(env, errorString);
    return nullptr;
  } catch (std::runtime_error &err) {
    std::string m1 = err.what();
    std::string errorString = "Error: " + m1;
    throwException(env, errorString);
    return nullptr;
  }
}
extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_transcodeImpl(JNIEnv *env, jobject thiz, jobject byteBuffer, jboolean allow_reconstruction) {
  auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
  int length = (int) env->GetDirectBufferCapacity(byteBuffer);
  if (!bufferAddress || length <= 0) {
    std::string errorString = "Only direct byte buffers are supported";
    throwException(env, errorString);
    return nullptr;
  }
  return transcode_jpeg_to_jxl(env, bufferAddress, length, allow_reconstruction, 0);
}