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

#include "JniExceptions.h"
#include <string>

jint throwImageSizeException(JNIEnv *env, const char* message) {
  jclass exClass;
  exClass = env->FindClass("com/awxkee/jxlcoder/InvalidImageSizeException");
  return env->ThrowNew(exClass, message);
}

jint throwInvalidJXLException(JNIEnv *env) {
  jclass exClass;
  exClass = env->FindClass("com/awxkee/jxlcoder/InvalidJXLException");
  return env->ThrowNew(exClass, "");
}

jint throwPixelsException(JNIEnv *env) {
  jclass exClass;
  exClass = env->FindClass("com/awxkee/jxlcoder/LockPixelsException");
  return env->ThrowNew(exClass, "");
}

jint throwException(JNIEnv *env, std::string &msg) {
  jclass exClass;
  exClass = env->FindClass("java/lang/Exception");
  return env->ThrowNew(exClass, msg.c_str());
}

jint throwCantCompressImage(JNIEnv *env) {
  jclass exClass;
  exClass = env->FindClass("com/awxkee/jxlcoder/JXLCoderCompressionException");
  return env->ThrowNew(exClass, "");
}

jint throwInvalidColorSpaceException(JNIEnv *env) {
  jclass exClass;
  exClass = env->FindClass("com/awxkee/jxlcoder/InvalidColorSpaceException");
  return env->ThrowNew(exClass, "");
}

jint throwInvalidCompressionOptionException(JNIEnv *env) {
  jclass exClass;
  exClass = env->FindClass("com/awxkee/jxlcoder/InvalidCompressionOptionException");
  return env->ThrowNew(exClass, "");
}

int androidOSVersion() {
  return android_get_device_api_level();
}