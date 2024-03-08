#include <jni.h>

/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 03/03/2024
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
#include <exception>
#include "JniExceptions.h"
#include "interop/JxlConstruction.hpp"
#include "interop/JxlReconstruction.hpp"

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_constructImpl(JNIEnv *env, jobject thiz, jbyteArray fromJpegData) {
  try {
    auto totalLength = env->GetArrayLength(fromJpegData);
    if (totalLength <= 0) {
      std::string errorString = "Invalid input array length";
      throwException(env, errorString);
      return nullptr;
    }
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(fromJpegData, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.data()));
    coder::JxlConstruction construction(srcBuffer);
    if (!construction.construct()) {
      std::string errorString = "Cannot construct JPEG XL from provided JPEG data";
      throwException(env, errorString);
      return nullptr;
    }
    std::vector<uint8_t> constructedJpegXL = construction.getCompressedData();
    jbyteArray byteArray = env->NewByteArray(static_cast<jint>(constructedJpegXL.size()));
    env->SetByteArrayRegion(byteArray, 0, static_cast<jint>(constructedJpegXL.size()),
                            reinterpret_cast<const jbyte *>(constructedJpegXL.data()));
    return byteArray;
  } catch (std::bad_alloc &err) {
    std::string errorString = "Not enough memory to construct this image";
    throwException(env, errorString);
    return nullptr;
  }
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_reconstructImpl(JNIEnv *env, jobject thiz, jbyteArray fromJpegXlData) {
  try {
    auto totalLength = env->GetArrayLength(fromJpegXlData);
    if (totalLength <= 0) {
      std::string errorString = "Invalid input array length";
      throwException(env, errorString);
      return nullptr;
    }
    std::vector<uint8_t> srcBuffer(totalLength);
    env->GetByteArrayRegion(fromJpegXlData, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.data()));
    coder::JxlReconstruction reconstruction(srcBuffer);
    if (!reconstruction.reconstruct()) {
      std::string errorString = "Cannot construct JPEG from provided JPEG XL data";
      throwException(env, errorString);
      return nullptr;
    }
    std::vector<uint8_t> constructedJPEG = reconstruction.getJPEGData();
    jbyteArray byteArray = env->NewByteArray(static_cast<jint>(constructedJPEG.size()));
    env->SetByteArrayRegion(byteArray, 0, static_cast<jint>(constructedJPEG.size()),
                            reinterpret_cast<const jbyte *>(constructedJPEG.data()));
    return byteArray;
  } catch (std::bad_alloc &err) {
    std::string errorString = "Not enough memory to re-construct this image";
    throwException(env, errorString);
    return nullptr;
  }
}