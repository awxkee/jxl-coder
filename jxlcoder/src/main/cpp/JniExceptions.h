//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#ifndef JXLCODER_JNIEXCEPTIONS_H
#define JXLCODER_JNIEXCEPTIONS_H

#include <jni.h>
#include <string>

jint throwInvalidJXLException(JNIEnv *env);
jint throwPixelsException(JNIEnv *env);
jint throwException(JNIEnv *env, std::string& msg);
jint throwCantCompressImage(JNIEnv *env);
jint throwInvalidColorSpaceException(JNIEnv *env);
jint throwInvalidCompressionOptionException(JNIEnv *env);

int androidOSVersion();

#endif //JXLCODER_JNIEXCEPTIONS_H
