#include <jni.h>
#include <string>
#include <vector>
#include <inttypes.h>
#include <libyuv.h>
#include "android/bitmap.h"
#include <android/log.h>
#include "jniExceptions.h"
#include "JxlEncoding.h"
#include "Rgba2Rgb.h"
#include "Rgb1010102toF16.h"
#include "colorspaces/bt709_colorspace.h"
#include "colorspaces/bt2020_colorspace.h"
#include "colorspaces/displayP3_HLG.h"
#include "colorspaces/linear_extended_bt2020.h"
#include <android/data_space.h>
#include "colorspaces/adobeRGB1998.h"
#include "colorspaces/dcip3.h"
#include "colorspaces/itur2020_HLG.h"
#include "colorspaces/bt2020_pq_colorspace.h"

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_encodeImpl(JNIEnv *env, jobject thiz, jobject bitmap,
                                             jint javaColorSpace, jint javaCompressionOption,
                                             jint effort, jstring bitmapColorProfile,
                                             jint dataSpace) {
    auto colorspace = static_cast<jxl_colorspace>(javaColorSpace);
    if (!colorspace) {
        throwInvalidColorSpaceException(env);
        return static_cast<jbyteArray>(nullptr);
    }
    auto compressionOption = static_cast<jxl_compression_option>(javaCompressionOption);
    if (!compressionOption) {
        throwInvalidCompressionOptionException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    if (effort < 0 || effort > 10) {
        throwInvalidCompressionOptionException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
        throwPixelsException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    if (info.flags & ANDROID_BITMAP_FLAGS_IS_HARDWARE) {
        throwHardwareBitmapException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 &&
        info.format != ANDROID_BITMAP_FORMAT_RGBA_F16 &&
        info.format != ANDROID_BITMAP_FORMAT_RGBA_1010102 &&
        info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
        std::string msg(
                "Currently support encoding only RGBA_8888, RGBA_F16, RGBA_1010102, RBR_565 images pixel format");
        throwException(env, msg);
        return static_cast<jbyteArray>(nullptr);
    }

    void *addr;
    if (AndroidBitmap_lockPixels(env, bitmap, &addr) != 0) {
        throwPixelsException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    std::vector<uint8_t> rgbaPixels(info.stride * info.height);
    memcpy(rgbaPixels.data(), addr, info.stride * info.height);
    AndroidBitmap_unlockPixels(env, bitmap);

    int imageStride = (int) info.stride;

    if (info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102) {
        std::vector<uint8_t> halfFloatPixels(info.width * sizeof(uint16_t) * 4 * info.height);
        coder::ConvertRGBA1010102toF16(reinterpret_cast<const uint8_t *>(rgbaPixels.data()),
                                       (int) info.stride,
                                       reinterpret_cast<uint16_t *>(halfFloatPixels.data()),
                                       (int) info.width * (int) sizeof(uint16_t) * 4,
                                       (int) info.width,
                                       (int) info.height);
        imageStride = (int) info.width * 4 * (int) sizeof(uint16_t);
        rgbaPixels = halfFloatPixels;
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        int newStride = (int) info.width * 4 * (int) sizeof(uint8_t);
        std::vector<uint8_t> rgba8888Pixels(newStride * info.height);
        libyuv::RGB565ToARGB(rgbaPixels.data(), (int) info.stride,
                             rgba8888Pixels.data(), newStride,
                             (int) info.width, (int) info.height);
        libyuv::ARGBToABGR(rgba8888Pixels.data(), newStride,
                           rgba8888Pixels.data(), newStride,
                           (int) info.width, (int) info.height);
        imageStride = newStride;
        rgbaPixels = rgba8888Pixels;
    }

    bool useFloat16 = info.format == ANDROID_BITMAP_FORMAT_RGBA_F16 ||
                      info.format == ANDROID_BITMAP_FORMAT_RGBA_1010102;

    std::vector<uint8_t> rgbPixels;
    switch (colorspace) {
        case rgb:
            rgbPixels.resize(info.width * info.height * 3 *
                             (useFloat16 ? sizeof(uint16_t) : sizeof(uint8_t)));
            if (useFloat16) {
                coder::Rgba16bit2RGB(reinterpret_cast<const uint16_t *>(rgbaPixels.data()),
                                     (int) info.stride,
                                     reinterpret_cast<uint16_t *>(rgbPixels.data()),
                                     (int) info.width * (int) sizeof(uint16_t) * 3,
                                     (int) info.height, (int) info.width);
            } else {
                libyuv::ARGBToRGB24(rgbaPixels.data(), static_cast<int>(imageStride),
                                    rgbPixels.data(),
                                    static_cast<int>(info.width * 3), static_cast<int>(info.width),
                                    static_cast<int>(info.height));
            }
            break;
        case rgba:
            rgbPixels.resize(imageStride * info.height);
            std::copy(rgbaPixels.begin(), rgbaPixels.end(), rgbPixels.begin());
            break;
    }
    rgbaPixels.clear();

    std::vector<uint8_t> compressedVector;

    std::vector<uint8_t> iccProfile;

    if (bitmapColorProfile) {
        const char *utf8String = env->GetStringUTFChars(bitmapColorProfile, nullptr);
        std::string stdString(utf8String);
        env->ReleaseStringUTFChars(bitmapColorProfile, utf8String);

        if (stdString == "Rec. ITU-R BT.709-5" || dataSpace == ADataSpace::ADATASPACE_BT709) {
            iccProfile.resize(sizeof(bt709));
            std::copy(&bt709[0], &bt709[0] + sizeof(bt709), iccProfile.begin());
        } else if (stdString == "Rec. ITU-R BT.2020-1" ||
                   dataSpace == ADataSpace::ADATASPACE_BT2020) {
            iccProfile.resize(sizeof(bt2020));
            std::copy(&bt2020[0], &bt2020[0] + sizeof(bt2020), iccProfile.begin());
        } else if (stdString == "Display P3" || dataSpace == ADataSpace::ADATASPACE_DISPLAY_P3) {
            iccProfile.resize(sizeof(displayP3_HLG));
            std::copy(&displayP3_HLG[0], &displayP3_HLG[0] + sizeof(displayP3_HLG),
                      iccProfile.begin());
        } else if (stdString == "sRGB IEC61966-2.1 (Linear)" ||
                   dataSpace == ADataSpace::ADATASPACE_SCRGB_LINEAR) {
            iccProfile.resize(sizeof(linearExtendedBT2020));
            std::copy(&linearExtendedBT2020[0],
                      &linearExtendedBT2020[0] + sizeof(linearExtendedBT2020), iccProfile.begin());
        } else if (stdString == "Perceptual Quantizer encoding" ||
                   dataSpace == ADataSpace::ADATASPACE_BT2020_ITU_PQ) {
            iccProfile.resize(sizeof(bt2020PQ));
            std::copy(&bt2020PQ[0], &bt2020PQ[0] + sizeof(bt2020PQ),
                      iccProfile.begin());
        } else if (stdString == "Adobe RGB (1998)" ||
                   dataSpace == ADataSpace::ADATASPACE_ADOBE_RGB) {
            iccProfile.resize(sizeof(adobeRGB1998));
            std::copy(&adobeRGB1998[0], &adobeRGB1998[0] + sizeof(adobeRGB1998),
                      iccProfile.begin());
        } else if (stdString == "SMPTE RP 431-2-2007 DCI (P3)" ||
                   dataSpace == ADataSpace::ADATASPACE_DCI_P3) {
            iccProfile.resize(sizeof(dcip3));
            std::copy(&dcip3[0], &dcip3[0] + sizeof(dcip3),
                      iccProfile.begin());
        }
    }

    if (!EncodeJxlOneshot(rgbPixels, info.width, info.height,
                          &compressedVector, colorspace,
                          compressionOption, useFloat16, iccProfile, effort)) {
        throwCantCompressImage(env);
        return static_cast<jbyteArray>(nullptr);
    }

    jbyteArray byteArray = env->NewByteArray((jsize) compressedVector.size());
    char *memBuf = (char *) ((void *) compressedVector.data());
    env->SetByteArrayRegion(byteArray, 0, (jint) compressedVector.size(),
                            reinterpret_cast<const jbyte *>(memBuf));
    compressedVector.clear();
    return byteArray;
}