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
#include "JxlDecoding.h"
#include "JniExceptions.h"
#include "colorspace.h"
#include "HalfFloats.h"
#include <libyuv.h>
#include "android/bitmap.h"
#include "Rgba16bitCopy.h"
#include "F32ToRGB1010102.h"
#include "SizeScaler.h"
#include "Support.h"
#include "ReformatBitmap.h"
#include "CopyUnaligned.h"
#include "XScaler.h"

jobject decodeSampledImageImpl(JNIEnv *env, std::vector<uint8_t> &imageData, jint scaledWidth,
                               jint scaledHeight,
                               jint javaPreferredColorConfig,
                               jint javaScaleMode, jint javaResizeFilter) {
    ScaleMode scaleMode;
    PreferredColorConfig preferredColorConfig;
    XSampler sampler;
    if (!checkDecodePreconditions(env, javaPreferredColorConfig, &preferredColorConfig,
                                  javaScaleMode, &scaleMode, javaResizeFilter, &sampler)) {
        return nullptr;
    }

    std::vector<uint8_t> rgbaPixels;
    std::vector<uint8_t> iccProfile;
    size_t xsize = 0, ysize = 0;
    bool useBitmapFloats = false;
    bool alphaPremultiplied = false;
    int osVersion = androidOSVersion();
    int bitDepth = 8;
    JxlOrientation jxlOrientation = JXL_ORIENT_IDENTITY;
    if (!DecodeJpegXlOneShot(reinterpret_cast<uint8_t *>(imageData.data()), imageData.size(),
                             &rgbaPixels,
                             &xsize, &ysize,
                             &iccProfile, &useBitmapFloats, &bitDepth, &alphaPremultiplied,
                             osVersion >= 26,
                             &jxlOrientation)) {
        throwInvalidJXLException(env);
        return nullptr;
    }

    if (jxlOrientation == JXL_ORIENT_ROTATE_90_CW || jxlOrientation == JXL_ORIENT_ROTATE_90_CCW ||
        jxlOrientation == JXL_ORIENT_ANTI_TRANSPOSE || jxlOrientation == JXL_ORIENT_TRANSPOSE) {
        size_t xz = xsize;
        xsize = ysize;
        ysize = xz;
    }

    imageData.clear();

    if (!iccProfile.empty()) {
        int stride = (int) xsize * 4 *
                     (int) (useBitmapFloats ? sizeof(uint16_t)
                                            : sizeof(uint8_t));
        convertUseDefinedColorSpace(rgbaPixels,
                                    stride,
                                    (int) xsize,
                                    (int) ysize, iccProfile.data(),
                                    iccProfile.size(),
                                    useBitmapFloats);
    }

    bool useSampler =
            (scaledWidth > 0 || scaledHeight > 0) && (scaledWidth != 0 && scaledHeight != 0);

    int finalWidth = (int) xsize;
    int finalHeight = (int) ysize;
    int stride = finalWidth * 4 * (int) (useBitmapFloats ? sizeof(uint16_t) : sizeof(uint8_t));

    if (useSampler) {
        auto scaleResult = RescaleImage(rgbaPixels, env, &stride, useBitmapFloats,
                                        reinterpret_cast<int *>(&finalWidth),
                                        reinterpret_cast<int *>(&finalHeight),
                                        scaledWidth, scaledHeight, alphaPremultiplied, scaleMode,
                                        sampler);
        if (!scaleResult) {
            return nullptr;
        }
    }

    std::string bitmapPixelConfig = useBitmapFloats ? "RGBA_F16" : "ARGB_8888";
    jobject hwBuffer = nullptr;
    ReformatColorConfig(env, rgbaPixels, bitmapPixelConfig, preferredColorConfig, bitDepth,
                        finalWidth, finalHeight, &stride, &useBitmapFloats,
                        &hwBuffer, alphaPremultiplied);

    if (bitmapPixelConfig == "HARDWARE") {
        jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
        jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "wrapHardwareBuffer",
                                                                "(Landroid/hardware/HardwareBuffer;Landroid/graphics/ColorSpace;)Landroid/graphics/Bitmap;");
        jobject emptyObject = nullptr;
        jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass,
                                                        createBitmapMethodID,
                                                        hwBuffer, emptyObject);
        return bitmapObj;
    }

    jclass bitmapConfig = env->FindClass("android/graphics/Bitmap$Config");
    jfieldID rgba8888FieldID = env->GetStaticFieldID(bitmapConfig,
                                                     bitmapPixelConfig.c_str(),
                                                     "Landroid/graphics/Bitmap$Config;");
    jobject rgba8888Obj = env->GetStaticObjectField(bitmapConfig, rgba8888FieldID);

    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
                                                    static_cast<jint>(finalWidth),
                                                    static_cast<jint>(finalHeight),
                                                    rgba8888Obj);

    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmapObj, &info) < 0) {
        throwPixelsException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    void *addr;
    if (AndroidBitmap_lockPixels(env, bitmapObj, &addr) != 0) {
        throwPixelsException(env);
        return static_cast<jobject>(nullptr);
    }

    if (bitmapPixelConfig == "RGB_565") {
        coder::CopyUnalignedRGB565(reinterpret_cast<const uint8_t *>(rgbaPixels.data()), stride,
                                   reinterpret_cast<uint8_t *>(addr), (int) info.stride,
                                   (int) info.width,
                                   (int) info.height);
    } else {
        coder::CopyUnalignedRGBA(reinterpret_cast<const uint8_t *>(rgbaPixels.data()), stride,
                                 reinterpret_cast<uint8_t *>(addr), (int) info.stride,
                                 (int) info.width,
                                 (int) info.height, useBitmapFloats ? 2 : 1);
    }


    if (AndroidBitmap_unlockPixels(env, bitmapObj) != 0) {
        throwPixelsException(env);
        return static_cast<jobject>(nullptr);
    }

    rgbaPixels.clear();

    return bitmapObj;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_decodeSampledImpl(JNIEnv *env, jobject thiz,
                                                    jbyteArray byte_array, jint scaledWidth,
                                                    jint scaledHeight,
                                                    jint javaPreferredColorConfig,
                                                    jint javaScaleMode,
                                                    jint resizeSampler) {
    try {
        auto totalLength = env->GetArrayLength(byte_array);
        std::vector<uint8_t> srcBuffer(totalLength);
        env->GetByteArrayRegion(byte_array, 0, totalLength,
                                reinterpret_cast<jbyte *>(srcBuffer.data()));
        return decodeSampledImageImpl(env, srcBuffer, scaledWidth, scaledHeight,
                                      javaPreferredColorConfig, javaScaleMode, resizeSampler);
    } catch (std::bad_alloc &err) {
        std::string errorString = "Not enough memory to decode this image";
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
                                                              jint scaleMode,
                                                              jint resizeSampler) {
    try {
        auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
        int length = (int) env->GetDirectBufferCapacity(byteBuffer);
        if (!bufferAddress || length <= 0) {
            std::string errorString = "Only direct byte buffers are supported";
            throwException(env, errorString);
            return nullptr;
        }
        std::vector<uint8_t> srcBuffer(length);
        std::copy(bufferAddress, bufferAddress + length, srcBuffer.begin());
        return decodeSampledImageImpl(env, srcBuffer, scaledWidth, scaledHeight,
                                      preferredColorConfig, scaleMode, resizeSampler);
    } catch (std::bad_alloc &err) {
        std::string errorString = "Not enough memory to decode this image";
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

    std::vector<uint8_t> icc_profile;
    size_t xsize = 0, ysize = 0;
    if (!DecodeBasicInfo(reinterpret_cast<uint8_t *>(srcBuffer.get()), totalLength, &xsize,
                         &ysize)) {
        return nullptr;
    }

    jclass sizeClass = env->FindClass("android/util/Size");
    jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
    auto sizeObject = env->NewObject(sizeClass, methodID, static_cast<jint >(xsize),
                                     static_cast<jint>(ysize));
    return sizeObject;
}