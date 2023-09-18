//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#include "JniExceptions.h"
#include <string>

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

jint throwException(JNIEnv *env, std::string& msg) {
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