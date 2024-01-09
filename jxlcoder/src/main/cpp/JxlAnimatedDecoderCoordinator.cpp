/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 08/01/2024
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

#include "JxlAnimatedDecoderCoordinator.h"
#include "SizeScaler.h"
#include "Support.h"
#include "JniExceptions.h"
#include <jni.h>
#include "JxlAnimatedDecoder.hpp"
#include "colorspace.h"
#include "android/bitmap.h"
#include "ReformatBitmap.h"
#include "CopyUnaligned.h"

using namespace std;

extern "C"
JNIEXPORT jlong JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_createCoordinator(JNIEnv *env, jobject thiz,
                                                            jobject byteBuffer,
                                                            jint javaPreferredColorConfig,
                                                            jint javaScaleMode,
                                                            jint javaJxlResizeSampler) {
    ScaleMode scaleMode;
    PreferredColorConfig preferredColorConfig;
    XSampler sampler;
    if (!checkDecodePreconditions(env, javaPreferredColorConfig, &preferredColorConfig,
                                  javaScaleMode, &scaleMode, javaJxlResizeSampler, &sampler)) {
        return 0;
    }

//    if (scaleWidth < 1 || scaleHeight < 1) {
//        std::string errorString =
//                "Invalid size provided, all sizes must be more than 0! Width: " +
//                std::to_string(scaleWidth) + ", height: " + std::to_string(scaleHeight);
//        throwException(env, errorString);
//        return 0;
//    }
    try {
        auto bufferAddress = reinterpret_cast<uint8_t *>(env->GetDirectBufferAddress(byteBuffer));
        int length = (int) env->GetDirectBufferCapacity(byteBuffer);
        if (!bufferAddress || length <= 0) {
            std::string errorString = "Only direct byte buffers are supported";
            throwException(env, errorString);
            return 0;
        }
        vector<uint8_t> srcBuffer(length);
        copy(bufferAddress, bufferAddress + length, srcBuffer.begin());
        JxlAnimatedDecoder *decoder = new JxlAnimatedDecoder(srcBuffer);
        JxlAnimatedDecoderCoordinator *coordinator = new JxlAnimatedDecoderCoordinator(
                decoder, scaleMode, preferredColorConfig, sampler
        );
        return reinterpret_cast<jlong >(coordinator);
    } catch (AnimatedDecoderError &err) {
        std::string errorString = err.what();
        throwException(env, errorString);
        return 0;
    } catch (std::bad_alloc &err) {
        std::string errorString = "OOM: " + string(err.what());
        throwException(env, errorString);
        return 0;
    }
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_createCoordinatorByteArray(JNIEnv *env, jobject thiz,
                                                                     jbyteArray byteArray,
                                                                     jint javaPreferredColorConfig,
                                                                     jint javaScaleMode,
                                                                     jint javaJxlResizeSampler) {
    ScaleMode scaleMode;
    PreferredColorConfig preferredColorConfig;
    XSampler sampler;
    if (!checkDecodePreconditions(env, javaPreferredColorConfig, &preferredColorConfig,
                                  javaScaleMode, &scaleMode, javaJxlResizeSampler, &sampler)) {
        return 0;
    }

//    if (scaleWidth < 1 || scaleHeight < 1) {
//        std::string errorString =
//                "Invalid size provided, all sizes must be more than 0! Width: " +
//                std::to_string(scaleWidth) + ", height: " + std::to_string(scaleHeight);
//        throwException(env, errorString);
//        return 0;
//    }
    try {
        auto length = env->GetArrayLength(byteArray);
        vector<uint8_t> srcBuffer(length);
        env->GetByteArrayRegion(byteArray, 0, length,
                                reinterpret_cast<jbyte *>(srcBuffer.data()));
        JxlAnimatedDecoder *decoder = new JxlAnimatedDecoder(srcBuffer);
        JxlAnimatedDecoderCoordinator *coordinator = new JxlAnimatedDecoderCoordinator(
                decoder, scaleMode, preferredColorConfig, sampler
        );
        return reinterpret_cast<jlong >(coordinator);
    } catch (AnimatedDecoderError &err) {
        std::string errorString = err.what();
        throwException(env, errorString);
        return 0;
    } catch (std::bad_alloc &err) {
        std::string errorString = "OOM: " + string(err.what());
        throwException(env, errorString);
        return 0;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_closeAndReleaseAnimatedImage(JNIEnv *env, jobject thiz,
                                                                       jlong coordinatorPtr) {
    JxlAnimatedDecoderCoordinator *coordinator = reinterpret_cast<JxlAnimatedDecoderCoordinator *>(coordinatorPtr);
    delete coordinator;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getNumberOfFrames(JNIEnv *env, jobject thiz,
                                                            jlong coordinatorPtr) {
    JxlAnimatedDecoderCoordinator *coordinator = reinterpret_cast<JxlAnimatedDecoderCoordinator *>(coordinatorPtr);
    return coordinator->numberOfFrames();
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getFrameDurationImpl(JNIEnv *env, jobject thiz,
                                                               jlong coordinatorPtr, jint frame) {
    JxlAnimatedDecoderCoordinator *coordinator = reinterpret_cast<JxlAnimatedDecoderCoordinator *>(coordinatorPtr);
    return coordinator->frameDuration(frame);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getLoopsCount(JNIEnv *env, jobject thiz,
                                                        jlong coordinatorPtr) {
    JxlAnimatedDecoderCoordinator *coordinator = reinterpret_cast<JxlAnimatedDecoderCoordinator *>(coordinatorPtr);
    return coordinator->loopsCount();
}
extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getFrameImpl(JNIEnv *env, jobject thiz,
                                                       jlong coordinatorPtr, jint frameIndex,
                                                       jint scaleWidth, jint scaleHeight) {
    try {
        JxlAnimatedDecoderCoordinator *coordinator = reinterpret_cast<JxlAnimatedDecoderCoordinator *>(coordinatorPtr);

        JxlFrame frame = coordinator->getFrame(frameIndex);
        vector<uint8_t> iccProfile = frame.iccProfile;
        vector<uint8_t> rgbaPixels = frame.pixels;
        // Currently always in 8bpp;
        bool useFloat16 = false;
        const int depth = 8;
        const bool alphaPremultiplied = coordinator->isAlphaAttenuated();
        if (!iccProfile.empty()) {
            int stride = (int) coordinator->getWidth() * 4 *
                         (int) (useFloat16 ? sizeof(uint16_t) : sizeof(uint8_t));
            convertUseDefinedColorSpace(rgbaPixels,
                                        stride,
                                        (int) coordinator->getWidth(),
                                        (int) coordinator->getHeight(), iccProfile.data(),
                                        iccProfile.size(),
                                        useFloat16);
        }
        int scaledWidth = scaleWidth;
        int scaledHeight = scaleHeight;
        bool useSampler =
                (scaledWidth > 0 || scaledHeight > 0) && (scaledWidth != 0 && scaledHeight != 0);

        int finalWidth = (int) coordinator->getWidth();
        int finalHeight = (int) coordinator->getHeight();
        int stride = finalWidth * 4 * (int) (useFloat16 ? sizeof(uint16_t) : sizeof(uint8_t));

        if (useSampler && scaledHeight > 0 && scaledWidth > 0) {
            auto scaleResult = RescaleImage(rgbaPixels, env, &stride, useFloat16,
                                            reinterpret_cast<int *>(&finalWidth),
                                            reinterpret_cast<int *>(&finalHeight),
                                            scaledWidth, scaledHeight, alphaPremultiplied,
                                            coordinator->getScaleMode(),
                                            coordinator->getSampler());
            if (!scaleResult) {
                return nullptr;
            }
        }

        std::string bitmapPixelConfig = useFloat16 ? "RGBA_F16" : "ARGB_8888";
        jobject hwBuffer = nullptr;
        ReformatColorConfig(env, rgbaPixels, bitmapPixelConfig,
                            coordinator->getPreferredColorConfig(), depth,
                            finalWidth, finalHeight, &stride, &useFloat16,
                            &hwBuffer, alphaPremultiplied);

        if (bitmapPixelConfig == "HARDWARE") {
            jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
            jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass,
                                                                    "wrapHardwareBuffer",
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
                                     (int) info.height, useFloat16 ? 2 : 1);
        }


        if (AndroidBitmap_unlockPixels(env, bitmapObj) != 0) {
            throwPixelsException(env);
            return static_cast<jobject>(nullptr);
        }

        rgbaPixels.clear();

        return bitmapObj;
    } catch (std::bad_alloc &err) {
        std::string errorString = "OOM: " + string(err.what());
        throwException(env, errorString);
        return 0;
    } catch (AnimatedDecoderError &err) {
        std::string errorString = err.what();
        throwException(env, errorString);
        return 0;
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getHeightImpl(JNIEnv *env, jobject thiz,
                                                        jlong coordinatorPtr) {
    JxlAnimatedDecoderCoordinator *coordinator = reinterpret_cast<JxlAnimatedDecoderCoordinator *>(coordinatorPtr);
    return coordinator->getHeight();
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedImage_getWidthImpl(JNIEnv *env, jobject thiz,
                                                       jlong coordinatorPtr) {
    JxlAnimatedDecoderCoordinator *coordinator = reinterpret_cast<JxlAnimatedDecoderCoordinator *>(coordinatorPtr);
    return coordinator->getWidth();
}