/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 16/09/2023
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
#include <android/bitmap.h>
#include <vector>
#include "JniExceptions.h"
#include "XScaler.h"
#include "Rgb1010102toF16.h"
#include "RGBAlpha.h"
#include "Rgb565.h"
#include "CopyUnaligned.h"

using namespace std;

extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_processing_BitmapProcessor_scaleImpl(JNIEnv *env, jobject thiz, jobject bitmap,
                                                      jint dst_width, jint dst_height,
                                                      jint scale_mode) {
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
            imageStride = (int) info.width * 4 * (int) sizeof(uint16_t);
            vector<uint8_t> halfFloatPixels(imageStride * info.height);
            coder::ConvertRGBA1010102toF16(reinterpret_cast<const uint8_t *>(rgbaPixels.data()),
                                           (int) info.stride,
                                           reinterpret_cast<uint16_t *>(halfFloatPixels.data()),
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
        } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
            coder::UnpremultiplyRGBA(rgbaPixels.data(), imageStride,
                                     rgbaPixels.data(), imageStride,
                                     (int) info.width,
                                     (int) info.height);
        }

        bool useFloat16 = info.format == ANDROID_BITMAP_FORMAT_RGBA_F16 ||
                          info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102;

        std::vector<uint8_t> rgbPixels;
        int requiredStride = (int) info.width * 4 *
                             (int) (useFloat16 ? sizeof(uint16_t) : sizeof(uint8_t));

        int dstStride = (int) dst_width * 4 * (int) (useFloat16 ? sizeof(uint16_t) : sizeof(uint8_t));

        if (requiredStride == info.stride) {
            rgbPixels = rgbaPixels;
        } else {
            rgbPixels.resize(requiredStride * (int) info.height);
            coder::CopyUnaligned(rgbaPixels.data(), imageStride, rgbPixels.data(),
                                 requiredStride, (int) info.width * 4, (int) info.height,
                                 (int) (useFloat16 ? sizeof(uint16_t)
                                                   : sizeof(uint8_t)));
        }
        imageStride = requiredStride;

        vector<uint8_t> output(dstStride * dst_height);

        if (useFloat16) {
            coder::scaleImageFloat16(
                    reinterpret_cast<uint16_t *>(rgbPixels.data()),
                    imageStride,
                    info.width,
                    info.height,
                    reinterpret_cast<uint16_t *>(output.data()),
                    dstStride,
                    dst_width,
                    dst_height,
                    4,
                    static_cast<XSampler>(scale_mode)
            );
        } else {
            coder::scaleImageU8(
                    rgbPixels.data(),
                    imageStride,
                    info.width,
                    info.height,
                    output.data(),
                    dstStride,
                    dst_width,
                    dst_height,
                    4,
                    8,
                    static_cast<XSampler>(scale_mode)
            );
        }

        rgbPixels.clear();

        std::string bitmapPixelConfig = useFloat16 ? "RGBA_F16" : "ARGB_8888";
        jclass bitmapConfig = env->FindClass("android/graphics/Bitmap$Config");
        jfieldID rgba8888FieldID = env->GetStaticFieldID(bitmapConfig,
                                                         bitmapPixelConfig.c_str(),
                                                         "Landroid/graphics/Bitmap$Config;");
        jobject rgba8888Obj = env->GetStaticObjectField(bitmapConfig, rgba8888FieldID);

        jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
        jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                                "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
        jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
                                                        static_cast<jint>(dst_width),
                                                        static_cast<jint>(dst_height),
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

        coder::CopyUnaligned(reinterpret_cast<const uint8_t *>(output.data()), dstStride,
                             reinterpret_cast<uint8_t *>(addr), (int) info.stride,
                             (int) info.width * 4,
                             (int) info.height, useFloat16 ? 2 : 1);

        if (AndroidBitmap_unlockPixels(env, bitmapObj) != 0) {
            throwPixelsException(env);
            return static_cast<jobject>(nullptr);
        }

        rgbaPixels.clear();

        return bitmapObj;
    } catch (bad_alloc& err) {
        std::string errorString = "Not enough memory to rescale the image";
        throwException(env, errorString);
        return nullptr;
    }
}