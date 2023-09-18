//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

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

extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_decodeSampledImpl(JNIEnv *env, jobject thiz,
                                                    jbyteArray byte_array, jint scaledWidth,
                                                    jint scaledHeight,
                                                    jint javaPreferredColorConfig,
                                                    jint javaScaleMode) {
    ScaleMode scaleMode;
    PreferredColorConfig preferredColorConfig;
    if (!checkDecodePreconditions(env, javaPreferredColorConfig, &preferredColorConfig,
                                  javaScaleMode, &scaleMode)) {
        return nullptr;
    }

    auto totalLength = env->GetArrayLength(byte_array);
    std::shared_ptr<void> srcBuffer(static_cast<char *>(malloc(totalLength)),
                                    [](void *b) { free(b); });
    env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.get()));

    std::vector<uint8_t> rgbaPixels;
    std::vector<uint8_t> iccProfile;
    size_t xsize = 0, ysize = 0;
    bool useBitmapFloats = false;
    bool alphaPremultiplied = false;
    int osVersion = androidOSVersion();
    int bitDepth = 8;
    if (!DecodeJpegXlOneShot(reinterpret_cast<uint8_t *>(srcBuffer.get()), totalLength, &rgbaPixels,
                             &xsize, &ysize,
                             &iccProfile, &useBitmapFloats, &bitDepth, &alphaPremultiplied,
                             osVersion >= 26)) {
        throwInvalidJXLException(env);
        return nullptr;
    }

    if (!iccProfile.empty()) {
        int stride = (int) xsize * 4 *
                     (int) (useBitmapFloats ? sizeof(uint32_t)
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
    int stride = finalWidth * 4 * (int) (useBitmapFloats ? sizeof(float) : sizeof(uint8_t));

    if (useSampler) {
        auto scaleResult = RescaleImage(rgbaPixels, env, &stride, useBitmapFloats,
                                        reinterpret_cast<int *>(&finalWidth),
                                        reinterpret_cast<int *>(&finalHeight),
                                        scaledWidth, scaledHeight, alphaPremultiplied, scaleMode);
        if (!scaleResult) {
            return nullptr;
        }
    }

    std::string bitmapPixelConfig = useBitmapFloats ? "RGBA_F16" : "ARGB_8888";
    jobject hwBuffer = nullptr;
    ReformatColorConfig(env, rgbaPixels, bitmapPixelConfig, preferredColorConfig, bitDepth,
                        finalWidth, finalHeight, &stride, &useBitmapFloats, &hwBuffer);     : "ARGB_8888");

    if (bitmapPixelConfig == "HARDWARE") {
        jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
        jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "wrapHardwareBuffer",
                                                                "(Landroid/hardware/HardwareBuffer;Landroid/graphics/ColorSpace;)Landroid/graphics/Bitmap;");
        jobject emptyObject = nullptr;
        jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
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
Java_com_awxkee_jxlcoder_JxlCoder_getSizeImpl(JNIEnv *env, jobject thiz, jbyteArray byte_array) {
    auto totalLength = env->GetArrayLength(byte_array);
    std::shared_ptr<void> srcBuffer(static_cast<char *>(malloc(totalLength)),
                                    [](void *b) { free(b); });
    env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.get()));

    std::vector<uint8_t> rgbaPixels;
    std::vector<uint8_t> icc_profile;
    size_t xsize = 0, ysize = 0;
    if (!DecodeBasicInfo(reinterpret_cast<uint8_t *>(srcBuffer.get()), totalLength, &rgbaPixels,
                         &xsize, &ysize)) {
        return nullptr;
    }

    jclass sizeClass = env->FindClass("android/util/Size");
    jmethodID methodID = env->GetMethodID(sizeClass, "<init>", "(II)V");
    auto sizeObject = env->NewObject(sizeClass, methodID, static_cast<jint >(xsize),
                                     static_cast<jint>(ysize));
    return sizeObject;
}