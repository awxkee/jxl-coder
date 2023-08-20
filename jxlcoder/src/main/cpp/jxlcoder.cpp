#include <jni.h>
#include <string>
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include <vector>
#include <inttypes.h>
#include "jxl/resizable_parallel_runner.h"
#include "jxl/resizable_parallel_runner_cxx.h"
#include "thread_parallel_runner.h"
#include "thread_parallel_runner_cxx.h"
#include <libyuv.h>
#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include "android/bitmap.h"

/**
 * Compresses the provided pixels.
 *
 * @param pixels input pixels
 * @param xsize width of the input image
 * @param ysize height of the input image
 * @param compressed will be populated with the compressed bytes
 */
bool EncodeJxlOneshot(const std::vector<uint8_t> &pixels, const uint32_t xsize,
                      const uint32_t ysize, std::vector<uint8_t> *compressed) {
    auto enc = JxlEncoderMake(/*memory_manager=*/nullptr);
    auto runner = JxlThreadParallelRunnerMake(
            /*memory_manager=*/nullptr,
                               JxlThreadParallelRunnerDefaultNumWorkerThreads());
    if (JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner(enc.get(),
                                                       JxlThreadParallelRunner,
                                                       runner.get())) {
        fprintf(stderr, "JxlEncoderSetParallelRunner failed\n");
        return false;
    }

    JxlPixelFormat pixel_format = {3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    JxlBasicInfo basic_info;
    JxlEncoderInitBasicInfo(&basic_info);
    basic_info.xsize = xsize;
    basic_info.ysize = ysize;
    basic_info.bits_per_sample = 32;
    basic_info.exponent_bits_per_sample = 8;
    basic_info.uses_original_profile = JXL_FALSE;
    basic_info.num_color_channels = 3;
    basic_info.alpha_bits = 0;
    if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc.get(), &basic_info)) {
        fprintf(stderr, "JxlEncoderSetBasicInfo failed\n");
        return false;
    }

    JxlColorEncoding color_encoding = {};
    JxlColorEncodingSetToSRGB(&color_encoding,
            /*is_gray=*/pixel_format.num_channels < 3);
    if (JXL_ENC_SUCCESS !=
        JxlEncoderSetColorEncoding(enc.get(), &color_encoding)) {
        fprintf(stderr, "JxlEncoderSetColorEncoding failed\n");
        return false;
    }

    JxlEncoderFrameSettings *frame_settings =
            JxlEncoderFrameSettingsCreate(enc.get(), nullptr);

    if (JXL_ENC_SUCCESS !=
        JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                (void *) pixels.data(),
                                sizeof(uint8_t) * pixels.size())) {
        fprintf(stderr, "JxlEncoderAddImageFrame failed\n");
        return false;
    }
    JxlEncoderCloseInput(enc.get());

    compressed->resize(64);
    uint8_t *next_out = compressed->data();
    size_t avail_out = compressed->size() - (next_out - compressed->data());
    JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
    while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
        process_result = JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
        if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
            size_t offset = next_out - compressed->data();
            compressed->resize(compressed->size() * 2);
            next_out = compressed->data() + offset;
            avail_out = compressed->size() - offset;
        }
    }
    compressed->resize(next_out - compressed->data());
    if (JXL_ENC_SUCCESS != process_result) {
        fprintf(stderr, "JxlEncoderProcessOutput failed\n");
        return false;
    }

    return true;
}

bool DecodeJpegXlOneShot(const uint8_t *jxl, size_t size,
                         std::vector<uint8_t> *pixels, size_t *xsize,
                         size_t *ysize, std::vector<uint8_t> *icc_profile) {
    // Multi-threaded parallel runner.
    auto runner = JxlResizableParallelRunnerMake(nullptr);

    auto dec = JxlDecoderMake(nullptr);
    if (JXL_DEC_SUCCESS !=
        JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO |
                                             JXL_DEC_COLOR_ENCODING |
                                             JXL_DEC_FULL_IMAGE)) {
        fprintf(stderr, "JxlDecoderSubscribeEvents failed\n");
        return false;
    }

    if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec.get(),
                                                       JxlResizableParallelRunner,
                                                       runner.get())) {
        fprintf(stderr, "JxlDecoderSetParallelRunner failed\n");
        return false;
    }

    JxlBasicInfo info;
    JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};

    JxlDecoderSetInput(dec.get(), jxl, size);
    JxlDecoderCloseInput(dec.get());

    for (;;) {
        JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());

        if (status == JXL_DEC_ERROR) {
            fprintf(stderr, "Decoder error\n");
            return false;
        } else if (status == JXL_DEC_NEED_MORE_INPUT) {
            fprintf(stderr, "Error, already provided all input\n");
            return false;
        } else if (status == JXL_DEC_BASIC_INFO) {
            if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {
                fprintf(stderr, "JxlDecoderGetBasicInfo failed\n");
                return false;
            }
            *xsize = info.xsize;
            *ysize = info.ysize;
            JxlResizableParallelRunnerSetThreads(
                    runner.get(),
                    JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
        } else if (status == JXL_DEC_COLOR_ENCODING) {
            // Get the ICC color profile of the pixel data
            size_t icc_size;
            if (JXL_DEC_SUCCESS !=
                JxlDecoderGetICCProfileSize(dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                                            &icc_size)) {
                fprintf(stderr, "JxlDecoderGetICCProfileSize failed\n");
                return false;
            }
            icc_profile->resize(icc_size);
            if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                    dec.get(), JXL_COLOR_PROFILE_TARGET_DATA,
                    icc_profile->data(), icc_profile->size())) {
                fprintf(stderr, "JxlDecoderGetColorAsICCProfile failed\n");
                return false;
            }
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t buffer_size;
            if (JXL_DEC_SUCCESS !=
                JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size)) {
                fprintf(stderr, "JxlDecoderImageOutBufferSize failed\n");
                return false;
            }
            if (buffer_size != *xsize * *ysize * 4) {
                fprintf(stderr, "Invalid out buffer size %" PRIu64 " %" PRIu64 "\n",
                        static_cast<uint64_t>(buffer_size),
                        static_cast<uint64_t>(*xsize * *ysize * 4));
                return false;
            }
            pixels->resize(*xsize * *ysize * 4);
            void *pixels_buffer = (void *) pixels->data();
            size_t pixels_buffer_size = pixels->size() * sizeof(uint8_t);
            if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(), &format,
                                                               pixels_buffer,
                                                               pixels_buffer_size)) {
                fprintf(stderr, "JxlDecoderSetImageOutBuffer failed\n");
                return false;
            }
        } else if (status == JXL_DEC_FULL_IMAGE) {
            // Nothing to do. Do not yet return. If the image is an animation, more
            // full frames may be decoded. This example only keeps the last one.
        } else if (status == JXL_DEC_SUCCESS) {
            // All decoding successfully finished.
            // It's not required to call JxlDecoderReleaseInput(dec.get()) here since
            // the decoder will be destroyed.
            return true;
        } else {
            fprintf(stderr, "Unknown decoder status\n");
            return false;
        }
    }
}

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

jint throwHardwareBitmapException(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/awxkee/jxlcoder/HardwareBitmapException");
    return env->ThrowNew(exClass, "");
}

jint throwInvalidPixelsFormat(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/awxkee/jxlcoder/InvalidPixelsFormatException");
    return env->ThrowNew(exClass, "");
}

jint throwCantCompressImage(JNIEnv *env) {
    jclass exClass;
    exClass = env->FindClass("com/awxkee/jxlcoder/JXLCoderCompressionException");
    return env->ThrowNew(exClass, "");
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_decodeImpl(JNIEnv *env, jobject thiz, jbyteArray byte_array) {
    auto totalLength = env->GetArrayLength(byte_array);
    std::shared_ptr<void> srcBuffer(static_cast<char *>(malloc(totalLength)),
                                    [](void *b) { free(b); });
    env->GetByteArrayRegion(byte_array, 0, totalLength, reinterpret_cast<jbyte *>(srcBuffer.get()));

    std::vector<uint8_t> rgbaPixels;
    std::vector<uint8_t> icc_profile;
    size_t xsize = 0, ysize = 0;
    if (!DecodeJpegXlOneShot(reinterpret_cast<uint8_t *>(srcBuffer.get()), totalLength, &rgbaPixels,
                             &xsize, &ysize,
                             &icc_profile)) {
        throwInvalidJXLException(env);
        return nullptr;
    }

    jclass bitmapConfig = env->FindClass("android/graphics/Bitmap$Config");
    jfieldID rgba8888FieldID = env->GetStaticFieldID(bitmapConfig, "ARGB_8888",
                                                     "Landroid/graphics/Bitmap$Config;");
    jobject rgba8888Obj = env->GetStaticObjectField(bitmapConfig, rgba8888FieldID);

    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapMethodID = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject bitmapObj = env->CallStaticObjectMethod(bitmapClass, createBitmapMethodID,
                                                    static_cast<jint>(xsize),
                                                    static_cast<jint>(ysize), rgba8888Obj);
    auto returningLength = rgbaPixels.size() / sizeof(uint32_t);
    jintArray pixels = env->NewIntArray((jsize) returningLength);

    libyuv::ABGRToARGB(rgbaPixels.data(), static_cast<int>(xsize * 4), rgbaPixels.data(),
                       static_cast<int>(xsize * 4), (int) xsize,
                       (int) ysize);

    env->SetIntArrayRegion(pixels, 0, (jsize) returningLength,
                           reinterpret_cast<const jint *>(rgbaPixels.data()));
    rgbaPixels.clear();
    jmethodID setPixelsMid = env->GetMethodID(bitmapClass, "setPixels", "([IIIIIII)V");
    env->CallVoidMethod(bitmapObj, setPixelsMid, pixels, 0,
                        static_cast<jint >(xsize), 0, 0,
                        static_cast<jint>(xsize),
                        static_cast<jint>(ysize));
    env->DeleteLocalRef(pixels);

    return bitmapObj;
}
extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_awxkee_jxlcoder_JxlCoder_encodeImpl(JNIEnv *env, jobject thiz, jobject bitmap) {
    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
        throwPixelsException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    if (info.flags & ANDROID_BITMAP_FLAGS_IS_HARDWARE) {
        throwHardwareBitmapException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        throwInvalidPixelsFormat(env);
        return static_cast<jbyteArray>(nullptr);
    }

    void *addr;
    if (AndroidBitmap_lockPixels(env, bitmap, &addr) != 0) {
        throwPixelsException(env);
        return static_cast<jbyteArray>(nullptr);
    }

    std::vector<uint8_t> rgbaPixels;
    rgbaPixels.resize(info.stride * info.height);
    memcpy(rgbaPixels.data(), addr, info.stride * info.height);
    AndroidBitmap_unlockPixels(env, bitmap);

    std::vector<uint8_t> rgbPixels;
    rgbPixels.resize(info.width * info.height * 3);
    libyuv::ARGBToRGB24(rgbaPixels.data(), static_cast<int>(info.stride), rgbPixels.data(),
                        static_cast<int>(info.width * 3), static_cast<int>(info.width),
                        static_cast<int>(info.height));
    rgbaPixels.clear();
    rgbaPixels.resize(1);

    std::vector<uint8_t> compressedVector;

    if (!EncodeJxlOneshot(rgbPixels, info.width, info.height, &compressedVector)) {
        throwCantCompressImage(env);
        return static_cast<jbyteArray>(nullptr);
    }

    rgbaPixels.clear();
    rgbaPixels.resize(1);

    jbyteArray byteArray = env->NewByteArray((jsize) compressedVector.size());
    char *memBuf = (char *) ((void *) compressedVector.data());
    env->SetByteArrayRegion(byteArray, 0, (jint) compressedVector.size(),
                            reinterpret_cast<const jbyte *>(memBuf));
    compressedVector.clear();
    return byteArray;
}