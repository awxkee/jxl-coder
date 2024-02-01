//
// Created by Radzivon Bartoshyk on 31/01/2024.
//

#include <jni.h>
#include <android/bitmap.h>
#include <vector>
#include "JniExceptions.h"
#include "XScaler.h"
#include "Rgb1010102.h"
#include "RGBAlpha.h"
#include "RgbaF16bitNBitU8.h"
#include "Rgb565.h"
#include "CopyUnaligned.h"
#include "processing/BoxBlur.h"

using namespace std;

extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_processing_BitmapProcessor_boxBlurImpl(JNIEnv *env, jobject thiz,
                                                           jobject bitmap, jint radius) {
    try {
        AndroidBitmapInfo info;
        if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
            throwPixelsException(env);
            return static_cast<jbyteArray>(nullptr);
        }

        if (info.flags & ANDROID_BITMAP_FLAGS_IS_HARDWARE) {
            std::string exc = "Hardware bitmap is not supported by JXL Coder";
            throwException(env, exc);
            return static_cast<jbyteArray>(nullptr);
        }

        if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 &&
            info.format != ANDROID_BITMAP_FORMAT_RGBA_F16 &&
            info.format != ANDROID_BITMAP_FORMAT_RGBA_1010102 &&
            info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
            string msg(
                    "Currently support encoding only RGBA_8888, RGBA_F16, RGBA_1010102, RBR_565 images pixel format");
            throwException(env, msg);
            return static_cast<jbyteArray>(nullptr);
        }

        void *addr = nullptr;
        if (AndroidBitmap_lockPixels(env, bitmap, &addr) != 0) {
            throwPixelsException(env);
            return static_cast<jbyteArray>(nullptr);
        }

        vector<uint8_t> rgbaPixels(info.stride * info.height);
        memcpy(rgbaPixels.data(), addr, info.stride * info.height);

        if (AndroidBitmap_unlockPixels(env, bitmap) != 0) {
            string exc = "Unlocking pixels has failed";
            throwException(env, exc);
            return static_cast<jbyteArray>(nullptr);
        }

        int imageStride = (int) info.stride;

        if (info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102) {
            imageStride = (int) info.width * 4 * (int) sizeof(uint8_t);
            vector<uint8_t> halfFloatPixels(imageStride * info.height);
            RGBA1010102ToU8(reinterpret_cast<const uint8_t *>(rgbaPixels.data()),
                            (int) info.stride,
                            reinterpret_cast<uint8_t *>(halfFloatPixels.data()),
                            (int) imageStride,
                            (int) info.width,
                            (int) info.height);
            rgbaPixels = halfFloatPixels;
        } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
            int newStride = (int) info.width * 4 * (int) sizeof(uint8_t);
            std::vector<uint8_t> rgba8888Pixels(newStride * info.height);
            coder::Rgb565ToUnsigned8(reinterpret_cast<const uint16_t *>(rgbaPixels.data()),
                                     (int) info.stride,
                                     rgba8888Pixels.data(), newStride,
                                     (int) info.width, (int) info.height, 8, 255);

            imageStride = newStride;
            rgbaPixels = rgba8888Pixels;
        } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_F16) {
            imageStride = (int) info.width * 4 * (int) sizeof(uint8_t);
            vector<uint8_t> halfFloatPixels(imageStride * info.height);
            coder::RGBAF16BitToNBitU8(reinterpret_cast<const uint16_t *>(rgbaPixels.data()),
                                      (int) info.stride,
                                      reinterpret_cast<uint8_t *>(halfFloatPixels.data()),
                                      (int) imageStride,
                                      (int) info.width,
                                      (int) info.height, 8, true);
            rgbaPixels = halfFloatPixels;
        }

        std::vector<uint8_t> rgbPixels;

        int lineWidth = info.width * sizeof(uint8_t) * 4;
        int alignment = 64;
        int padding = (alignment - (lineWidth % alignment)) % alignment;
        int dstStride = (int) info.width * 4 * (int) sizeof(uint8_t) + padding;

        rgbPixels.resize(dstStride * (int) info.height);
        coder::CopyUnaligned(rgbaPixels.data(), imageStride, rgbPixels.data(),
                             dstStride, (int) info.width * 4, (int) info.height,
                             sizeof(uint8_t));

        imageStride = dstStride;

        coder::boxBlurU8(rgbPixels.data(), dstStride, info.width, info.height, radius);

        std::string bitmapPixelConfig = "ARGB_8888";
        jclass bitmapConfig = env->FindClass("android/graphics/Bitmap$Config");
        jfieldID rgba8888FieldID = env->GetStaticFieldID(bitmapConfig,
                                                         bitmapPixelConfig.c_str(),
                                                         "Landroid/graphics/Bitmap$Config;");
        jobject rgba8888Obj = env->GetStaticObjectField(bitmapConfig, rgba8888FieldID);

        jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
        jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                                "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
        jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
                                                        static_cast<jint>(info.width),
                                                        static_cast<jint>(info.height),
                                                        rgba8888Obj);


        if (AndroidBitmap_getInfo(env, bitmapObj, &info) < 0) {
            throwPixelsException(env);
            return static_cast<jbyteArray>(nullptr);
        }

        addr = nullptr;

        if (AndroidBitmap_lockPixels(env, bitmapObj, &addr) != 0) {
            throwPixelsException(env);
            return static_cast<jobject>(nullptr);
        }

        coder::CopyUnaligned(reinterpret_cast<const uint8_t *>(rgbPixels.data()), dstStride,
                             reinterpret_cast<uint8_t *>(addr), (int) info.stride,
                             (int) info.width * 4,
                             (int) info.height, 1);

        if (AndroidBitmap_unlockPixels(env, bitmapObj) != 0) {
            throwPixelsException(env);
            return static_cast<jobject>(nullptr);
        }

        rgbaPixels.clear();

        return bitmapObj;
    } catch (bad_alloc &err) {
        std::string errorString = "Not enough memory to rescale the image";
        throwException(env, errorString);
        return nullptr;
    }
}