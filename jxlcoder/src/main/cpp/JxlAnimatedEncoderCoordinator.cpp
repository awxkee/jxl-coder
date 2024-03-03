/*
 * MIT License
 *
 * Copyright (c) 2023 Radzivon Bartoshyk
 * jxl-coder [https://github.com/awxkee/jxl-coder]
 *
 * Created by Radzivon Bartoshyk on 09/01/2024
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

#include "JxlAnimatedEncoderCoordinator.h"
#include <jni.h>
#include "JxlDefinitions.h"
#include "Support.h"
#include <string>
#include <vector>
#include "JniExceptions.h"
#include <android/data_space.h>
#include <android/bitmap.h>
#include "conversion/Rgb1010102toF16.h"
#include <libyuv.h>
#include "conversion/Rgba8ToF16.h"
#include "conversion/RGBAlpha.h"
#include "conversion/Rgb1010102.h"
#include "conversion/RgbaF16bitNBitU8.h"
#include "conversion/Rgba2Rgb.h"
#include "conversion/Rgb565.h"
#include "CopyUnaligned.h"

using namespace std;

extern "C"
JNIEXPORT void JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedEncoder_closeAndReleaseAnimatedEncoder(JNIEnv *env,
                                                                           jobject thiz,
                                                                           jlong coordinatorPtr) {
    JxlAnimatedEncoderCoordinator *coordinator = reinterpret_cast<JxlAnimatedEncoderCoordinator *>(coordinatorPtr);
    delete coordinator;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedEncoder_createEncodeCoordinator(JNIEnv *env, jobject thiz,
                                                                    jint width, jint height,
                                                                    jint numLoops,
                                                                    jint javaColorSpace,
                                                                    jint javaCompressionOption,
                                                                    jint jQuality,
                                                                    jint javaDataSpaceValue,
                                                                    jint effort,
                                                                    jint decodingSpeed,
                                                                    jint javaDataPixelFormat) {
    auto colorspace = static_cast<JxlColorPixelType>(javaColorSpace);
    if (!colorspace) {
        throwInvalidColorSpaceException(env);
        return 0;
    }
    auto compressionOption = static_cast<JxlCompressionOption>(javaCompressionOption);
    if (!compressionOption) {
        throwInvalidCompressionOptionException(env);
        return 0;
    }

    if (effort < 0 || effort > 10) {
        throwInvalidCompressionOptionException(env);
        return 0;
    }

    if (jQuality < 0 || jQuality > 100) {
        std::string exc = "Quality must be in 0...100";
        throwException(env, exc);
        return 0;
    }

    auto dataPixelFormat = static_cast<JxlEncodingPixelDataFormat>(javaDataPixelFormat);

    JxlColorMatrix colorSpaceMatrix = MATRIX_UNKNOWN;

    if (javaDataSpaceValue > 0) {
        int dataSpace = javaDataSpaceValue;
        if (dataSpace == ADataSpace::ADATASPACE_BT709) {
            colorSpaceMatrix = MATRIX_ITUR_709;
        } else if (dataSpace == ADataSpace::ADATASPACE_BT2020) {
            colorSpaceMatrix = MATRIX_ITUR_2020;
        } else if (dataSpace == ADataSpace::ADATASPACE_DISPLAY_P3) {
            colorSpaceMatrix = MATRIX_DISPLAY_P3;
        } else if (dataSpace == ADataSpace::ADATASPACE_SCRGB_LINEAR) {
            colorSpaceMatrix = MATRIX_SRGB_LINEAR;
        } else if (dataSpace == ADataSpace::ADATASPACE_BT2020_ITU_PQ) {
            colorSpaceMatrix = MATRIX_ITUR_2020_PQ;
        } else if (dataSpace == ADataSpace::ADATASPACE_ADOBE_RGB) {
            colorSpaceMatrix = MATRIX_ADOBE_RGB;
        } else if (dataSpace == ADataSpace::ADATASPACE_DCI_P3) {
            colorSpaceMatrix = MATRIX_DCI_P3;
        }
    }

    try {
        JxlAnimatedEncoder *encoder = new JxlAnimatedEncoder(width, height, colorspace,
                                                             dataPixelFormat,
                                                             compressionOption, numLoops, jQuality,
                                                             effort, decodingSpeed);
        JxlAnimatedEncoderCoordinator *coordinator = new JxlAnimatedEncoderCoordinator(encoder,
                                                                                       colorspace,
                                                                                       compressionOption,
                                                                                       effort,
                                                                                       jQuality,
                                                                                       colorSpaceMatrix,
                                                                                       dataPixelFormat);
        return reinterpret_cast<jlong >(coordinator);
    } catch (std::bad_alloc &err) {
        std::string errorString = "OOM: " + string(err.what());
        throwException(env, errorString);
        return 0;
    } catch (AnimatedEncoderError &err) {
        std::string errorString = err.what();
        throwException(env, errorString);
        return 0;
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedEncoder_addFrameImpl(JNIEnv *env, jobject thiz,
                                                         jlong coordinatorPtr, jobject bitmap,
                                                         jint duration) {
    try {
        JxlAnimatedEncoderCoordinator *coordinator = reinterpret_cast<JxlAnimatedEncoderCoordinator *>(coordinatorPtr);

        AndroidBitmapInfo info;
        if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
            throwPixelsException(env);
            return;
        }

        if (info.width != coordinator->getWidth() || info.height != coordinator->getHeight()) {
            std::string exc = "Bounds of each frames must be the same to origin (" +
                              to_string(coordinator->getWidth()) + "," +
                              to_string(coordinator->getHeight()) + "), but were provided (" +
                              to_string(info.width) + ", " +
                              to_string(info.height) + ")";
            throwException(env, exc);
            return;
        }

        if (info.flags & ANDROID_BITMAP_FLAGS_IS_HARDWARE) {
            std::string exc = "Hardware bitmap is not supported by JXL Coder";
            throwException(env, exc);
            return;
        }

        if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 &&
            info.format != ANDROID_BITMAP_FORMAT_RGBA_F16 &&
            info.format != ANDROID_BITMAP_FORMAT_RGBA_1010102 &&
            info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
            string msg(
                    "Currently support encoding only RGBA_8888, RGBA_F16, RGBA_1010102, RBR_565 images pixel format");
            throwException(env, msg);
            return;
        }

        void *addr;
        if (AndroidBitmap_lockPixels(env, bitmap, &addr) != 0) {
            throwPixelsException(env);
            return;
        }

        vector<uint8_t> rgbaPixels(info.stride * info.height);
        memcpy(rgbaPixels.data(), addr, info.stride * info.height);

        if (AndroidBitmap_unlockPixels(env, bitmap) != 0) {
            string exc = "Unlocking pixels has failed";
            throwException(env, exc);
            return;
        }

        JxlEncodingPixelDataFormat dataPixelFormat = coordinator->getDataPixelFormat();
        int imageStride = (int) info.stride;
        if (info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102) {
            if (dataPixelFormat == BINARY_16) {
                imageStride = (int) info.width * 4 * (int) sizeof(uint16_t);
                vector<uint8_t> halfFloatPixels(imageStride * info.height);
                coder::ConvertRGBA1010102toF16(reinterpret_cast<const uint8_t *>(rgbaPixels.data()),
                                               (int) info.stride,
                                               reinterpret_cast<uint16_t *>(halfFloatPixels.data()),
                                               (int) imageStride,
                                               (int) info.width,
                                               (int) info.height);
                rgbaPixels = halfFloatPixels;
            } else {
                imageStride = (int) info.width * 4 * (int) sizeof(uint8_t);
                vector<uint8_t> u8PixelsData(imageStride * info.height);
                RGBA1010102ToU8(reinterpret_cast<const uint8_t *>(rgbaPixels.data()),
                                (int) info.stride,
                                reinterpret_cast<uint8_t *>(u8PixelsData.data()),
                                (int) imageStride,
                                (int) info.width,
                                (int) info.height);
                rgbaPixels = u8PixelsData;
            }
        } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
            int newStride = (int) info.width * 4 * (int) sizeof(uint8_t);
            vector<uint8_t> rgba8888Pixels(newStride * info.height);
            if (dataPixelFormat == UNSIGNED_8) {
                coder::Rgb565ToUnsigned8(reinterpret_cast<uint16_t *>(rgbaPixels.data()),
                                         (int) info.stride,
                                         rgba8888Pixels.data(), newStride,
                                         (int) info.width, (int) info.height, 8, 255);
                imageStride = newStride;
                rgbaPixels = rgba8888Pixels;
            } else {
                int b16Stride = (int) info.width * 4 * (int) sizeof(uint16_t);
                vector<uint8_t> halfFloatPixels(imageStride * info.height);
                coder::Rgb565ToF16(reinterpret_cast<uint16_t *>(rgbaPixels.data()), newStride,
                                   reinterpret_cast<uint16_t *>(halfFloatPixels.data()), b16Stride,
                                   (int) info.width, (int) info.height);
                imageStride = b16Stride;
                rgbaPixels = halfFloatPixels;
            }
        } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
            if (dataPixelFormat == UNSIGNED_8) {
                coder::UnpremultiplyRGBA(rgbaPixels.data(), imageStride,
                                         rgbaPixels.data(), imageStride,
                                         (int) info.width,
                                         (int) info.height);
            } else if (dataPixelFormat == BINARY_16) {
                int b16Stride = (int) info.width * 4 * (int) sizeof(uint16_t);
                vector<uint8_t> halfFloatPixels(imageStride * info.height);
                coder::Rgba8ToF16(rgbaPixels.data(), imageStride,
                                  reinterpret_cast<uint16_t *>(halfFloatPixels.data()), b16Stride,
                                  (int) info.width, (int) info.height, 8, true);
                imageStride = b16Stride;
                rgbaPixels = halfFloatPixels;
            }
        } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_F16) {
            if (dataPixelFormat == UNSIGNED_8) {
                int b16Stride = (int) info.width * 4 * (int) sizeof(uint8_t);
                vector<uint8_t> u8PixelsData(imageStride * info.height);
                coder::RGBAF16BitToNBitU8(reinterpret_cast<uint16_t *>(rgbaPixels.data()),
                                          imageStride,
                                          reinterpret_cast<uint8_t *>(u8PixelsData.data()),
                                          b16Stride,
                                          (int) info.width, (int) info.height, 8, true);
                imageStride = b16Stride;
                rgbaPixels = u8PixelsData;
            }
        }

        std::vector<uint8_t> rgbPixels;
        JxlColorPixelType colorPixelType = coordinator->getColorPixelType();
        switch (colorPixelType) {
            case rgb: {
                int requiredStride = (int) info.width * 3 *
                                     (int) (dataPixelFormat == BINARY_16 ? sizeof(uint16_t)
                                                                         : sizeof(uint8_t));
                rgbPixels.resize(info.height * requiredStride);
                if (dataPixelFormat == BINARY_16) {
                    coder::Rgba16bit2RGB(reinterpret_cast<const uint16_t *>(rgbaPixels.data()),
                                         (int) imageStride,
                                         reinterpret_cast<uint16_t *>(rgbPixels.data()),
                                         (int) requiredStride,
                                         (int) info.height, (int) info.width);
                } else {
                    libyuv::ARGBToRGB24(rgbaPixels.data(), static_cast<int>(imageStride),
                                        rgbPixels.data(),
                                        static_cast<int>(requiredStride),
                                        static_cast<int>(info.width),
                                        static_cast<int>(info.height));
                }
                imageStride = requiredStride;
            }
                break;
            case rgba: {
                int requiredStride = (int) info.width * 4 *
                                     (int) (dataPixelFormat == BINARY_16 ? sizeof(uint16_t)
                                                                         : sizeof(uint8_t));
                if (requiredStride == imageStride) {
                    rgbPixels = rgbaPixels;
                } else {
                    rgbPixels.resize(requiredStride * (int) info.height);
                    coder::CopyUnaligned(rgbaPixels.data(), imageStride, rgbPixels.data(),
                                         requiredStride, (int) info.width * 4, (int) info.height,
                                         (int) (dataPixelFormat == BINARY_16 ? sizeof(uint16_t)
                                                                             : sizeof(uint8_t)));
                }
                imageStride = requiredStride;
            }
                break;
        }

        rgbaPixels.clear();

        JxlAnimatedEncoder *encoder = coordinator->getEncoder();
        encoder->addFrame(rgbPixels, duration);
    } catch (std::bad_alloc &err) {
        std::string errorString = "OOM: " + string(err.what());
        throwException(env, errorString);
        return;
    } catch (AnimatedEncoderError &err) {
        std::string errorString = err.what();
        throwException(env, errorString);
        return;
    }
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_awxkee_jxlcoder_JxlAnimatedEncoder_encodeAnimatedImpl(JNIEnv *env, jobject thiz,
                                                               jlong coordinatorPtr) {
    try {
        JxlAnimatedEncoderCoordinator *coordinator = reinterpret_cast<JxlAnimatedEncoderCoordinator *>(coordinatorPtr);
        vector<uint8_t> buffer;
        coordinator->finish(ref(buffer));
        jbyteArray byteArray = env->NewByteArray((jsize) buffer.size());
        char *memBuf = (char *) ((void *) buffer.data());
        env->SetByteArrayRegion(byteArray, 0, (jint) buffer.size(),
                                reinterpret_cast<const jbyte *>(memBuf));
        buffer.clear();
        return byteArray;
    } catch (std::bad_alloc &err) {
        std::string errorString = "OOM: " + string(err.what());
        throwException(env, errorString);
        return static_cast<jbyteArray >(nullptr);
    } catch (AnimatedEncoderError &err) {
        std::string errorString = err.what();
        throwException(env, errorString);
        return static_cast<jbyteArray >(nullptr);
    }
}