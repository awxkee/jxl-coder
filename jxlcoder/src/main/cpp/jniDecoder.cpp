//
// Created by Radzivon Bartoshyk on 04/09/2023.
//

#include <jni.h>
#include <vector>
#include "jxlDecoding.h"
#include "jniExceptions.h"
#include "colorspace.h"
#include "halfFloats.h"
#include "stb_image_resize.h"
#include <libyuv.h>
#include "android/bitmap.h"

int androidOSVersion() {
    return android_get_device_api_level();
}

void copyRGBA16(std::vector<uint8_t> &source, int srcStride, uint8_t *destination, int dstStride,
                int width, int height) {
    auto src = reinterpret_cast<uint8_t *>(source.data());
    auto dst = reinterpret_cast<uint8_t *>(destination);

    for (int y = 0; y < height; ++y) {

        auto srcPtr = reinterpret_cast<uint16_t *>(src);
        auto dstPtr = reinterpret_cast<uint16_t *>(dst);

        for (int x = 0; x < width; ++x) {
            auto srcPtr64 = reinterpret_cast<uint64_t *>(srcPtr);
            auto dstPtr64 = reinterpret_cast<uint64_t *>(dstPtr);
            dstPtr64[0] = srcPtr64[0];
            srcPtr += 4;
            dstPtr += 4;
        }

        src += srcStride;
        dst += dstStride;
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_decodeSampledImpl(JNIEnv *env, jobject thiz,
                                                    jbyteArray byte_array, jint scaledWidth,
                                                    jint scaledHeight) {
    auto totalLength = env->GetArrayLength(byte_array);
    std::shared_ptr<void> srcBuffer(static_cast<char *>(malloc(totalLength)),
                                    [](void *b) { free(b); });
    env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.get()));

    std::vector<uint8_t> rgbaPixels;
    std::vector<uint8_t> iccProfile;
    size_t xsize = 0, ysize = 0;
    bool useBitmapFloats = false;
    bool alphaPremultiplied = false;
    if (!DecodeJpegXlOneShot(reinterpret_cast<uint8_t *>(srcBuffer.get()), totalLength, &rgbaPixels,
                             &xsize, &ysize,
                             &iccProfile, &useBitmapFloats, &alphaPremultiplied,
                             androidOSVersion() >= 26)) {
        throwInvalidJXLException(env);
        return nullptr;
    }

    bool useSampler = scaledWidth > 0 && scaledHeight > 0;

    int finalWidth = useSampler ? (int) scaledWidth : (int) xsize;
    int finalHeight = useSampler ? (int) scaledHeight : (int) ysize;

    jclass bitmapConfig = env->FindClass("android/graphics/Bitmap$Config");
    jfieldID rgba8888FieldID = env->GetStaticFieldID(bitmapConfig,
                                                     useBitmapFloats ? "RGBA_F16" : "ARGB_8888",
                                                     "Landroid/graphics/Bitmap$Config;");
    jobject rgba8888Obj = env->GetStaticObjectField(bitmapConfig, rgba8888FieldID);

    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
                                                    static_cast<jint>(finalWidth),
                                                    static_cast<jint>(finalHeight),
                                                    rgba8888Obj);

    if (!iccProfile.empty()) {
        convertUseDefinedColorSpace(rgbaPixels, (int) xsize * 4 *
                                                (int) (useBitmapFloats ? sizeof(uint32_t)
                                                                       : sizeof(uint8_t)),
                                    (int) xsize,
                                    (int) ysize, iccProfile.data(),
                                    iccProfile.size(),
                                    useBitmapFloats);
    }

    if (useSampler) {
        std::vector<uint8_t> newImageData(finalWidth * finalHeight * 4 *
                                          (useBitmapFloats ? sizeof(uint32_t) : sizeof(uint8_t)));

        if (useBitmapFloats) {
            // We we'll use Mitchell because we want less artifacts in our HDR image
            int result = stbir_resize_float_generic(
                    reinterpret_cast<const float *>(rgbaPixels.data()), (int) xsize,
                    (int) ysize, 0,
                    reinterpret_cast<float *>(newImageData.data()), scaledWidth,
                    scaledHeight, 0,
                    4, 3, alphaPremultiplied ? STBIR_FLAG_ALPHA_PREMULTIPLIED : 0,
                    STBIR_EDGE_CLAMP, STBIR_FILTER_MITCHELL, STBIR_COLORSPACE_SRGB,
                    nullptr
            );
            if (result != 1) {
                std::string s("Failed to resample an image");
                throwException(env, s);
                return static_cast<jobject>(nullptr);
            }
        } else {
            libyuv::ARGBScale(rgbaPixels.data(), static_cast<int>(xsize * 4),
                              static_cast<int>(xsize),
                              static_cast<int>(ysize),
                              newImageData.data(), scaledWidth * 4, scaledWidth, scaledHeight,
                              libyuv::kFilterBilinear);
        }

        rgbaPixels.clear();
        rgbaPixels = newImageData;
    }

    if (useBitmapFloats) {
        std::vector<uint8_t> newImageData(finalWidth * finalHeight * 4 * sizeof(uint16_t));
        auto startPixels = reinterpret_cast<float *>(rgbaPixels.data());
        auto dstPixels = reinterpret_cast<uint16_t *>(newImageData.data());
#if HAVE_NEON
        RGBAfloat32_to_float16_NEON(startPixels, (int)finalWidth * 4 * (int)sizeof(uint32_t),
                                    dstPixels,(int)finalWidth * 4 * (int)sizeof(uint16_t),
                                    finalWidth, finalHeight);
#else
        RGBAfloat32_to_float16(startPixels, dstPixels, finalHeight * finalWidth * 4);
#endif
        rgbaPixels.clear();
        rgbaPixels = newImageData;
    }

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

    if (useBitmapFloats) {
        copyRGBA16(rgbaPixels, finalWidth * 4 * (int) sizeof(uint16_t),
                   reinterpret_cast<uint8_t *>(addr), (int)info.stride, (int)info.width, (int)info.height);
    } else {
        libyuv::ARGBCopy(reinterpret_cast<uint8_t *>(rgbaPixels.data()),
                         (int) finalHeight * 4 * (int) sizeof(uint8_t),
                         reinterpret_cast<uint8_t *>(addr), (int) info.stride, (int) info.width,
                         (int) info.height);
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