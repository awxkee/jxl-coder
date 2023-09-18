//
// Created by Radzivon Bartoshyk on 16/09/2023.
//

#include "ReformatBitmap.h"
#include <string>
#include "Support.h"
#include "HalfFloats.h"
#include "Rgb565.h"
#include <android/bitmap.h>
#include <HardwareBuffersCompat.h>
#include <mutex>
#include "Rgb1010102.h"
#include "F32toU8.h"
#include "Rgba8ToF16.h"
#include "CopyUnaligned.h"
#include "JniExceptions.h"

void
ReformatColorConfig(JNIEnv *env, std::vector<uint8_t> &imageData, std::string &imageConfig,
                    PreferredColorConfig preferredColorConfig, int depth,
                    int imageWidth, int imageHeight, int *stride, bool *useFloats,
                    jobject *hwBuffer) {
    *hwBuffer = nullptr;
    if (preferredColorConfig == Default) {
        int osVersion = androidOSVersion();
        if (depth > 8 && osVersion >= 26) {
            if (osVersion >= 33) {
                preferredColorConfig = Rgba_1010102;
            } else {
                preferredColorConfig = Rgba_F16;
            }
        } else {
            preferredColorConfig = Rgba_8888;
        }
    }
    switch (preferredColorConfig) {
        case Rgba_8888:
            if (*useFloats) {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::vector<uint8_t> rgba8888Data(dstStride * imageHeight);
                coder::F32toU8(reinterpret_cast<const float *>(imageData.data()),
                               *stride, rgba8888Data.data(), dstStride, imageWidth,
                               imageHeight, 8);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "ARGB_8888";
                imageData = rgba8888Data;
            }
            break;
        case Rgba_F16:
            if (*useFloats) {
                int dstStride = imageWidth * 4 * (int) sizeof(uint16_t);
                std::vector<uint8_t> rgbaF16Data(dstStride * imageHeight);
                coder::RgbaF32ToF16(reinterpret_cast<const float *>(imageData.data()),
                                    (int) *stride,
                                    reinterpret_cast<uint16_t *>(rgbaF16Data.data()),
                                    dstStride, imageWidth, imageHeight);

                *stride = dstStride;
                *useFloats = true;
                imageConfig = "RGBA_F16";
                imageData = rgbaF16Data;
                break;
            } else {
                int dstStride = imageWidth * 4 * (int) sizeof(uint16_t);
                std::vector<uint8_t> rgbaF16Data(dstStride * imageHeight);
                coder::Rgba8ToF16(imageData.data(), *stride,
                                  reinterpret_cast<uint16_t *>(rgbaF16Data.data()), dstStride,
                                  imageWidth, imageHeight, depth);
                *stride = dstStride;
                *useFloats = true;
                imageConfig = "RGBA_F16";
                imageData = rgbaF16Data;
            }
            break;
        case Rgb_565:
            if (*useFloats) {
                int dstStride = imageWidth * (int) sizeof(uint16_t);
                std::vector<uint8_t> rgb565Data(dstStride * imageHeight);
                coder::RGBAF32To565(reinterpret_cast<const float *>(imageData.data()), *stride,
                                    reinterpret_cast<uint16_t *>(rgb565Data.data()), dstStride,
                                    imageWidth, imageHeight);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGB_565";
                imageData = rgb565Data;
                break;
            } else {
                int dstStride = imageWidth * (int) sizeof(uint16_t);
                std::vector<uint8_t> rgb565Data(dstStride * imageHeight);
                coder::Rgba8To565(imageData.data(), *stride,
                                  reinterpret_cast<uint16_t *>(rgb565Data.data()), dstStride,
                                  imageWidth, imageHeight, depth);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGB_565";
                imageData = rgb565Data;
            }
            break;
        case Rgba_1010102:
            if (*useFloats) {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::vector<uint8_t> rgba1010102Data(dstStride * imageHeight);
                coder::F32ToRGBA1010102(reinterpret_cast<const float *>(imageData.data()),
                                        *stride,
                                        reinterpret_cast<uint8_t *>(rgba1010102Data.data()),
                                        dstStride,
                                        imageWidth, imageHeight);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGBA_1010102";
                imageData = rgba1010102Data;
                break;
            } else {
                int dstStride = imageWidth * 4 * (int) sizeof(uint8_t);
                std::vector<uint8_t> rgba1010102Data(dstStride * imageHeight);
                coder::Rgba8ToRGBA1010102(reinterpret_cast<const uint8_t *>(imageData.data()),
                                          *stride,
                                          reinterpret_cast<uint8_t *>(rgba1010102Data.data()),
                                          dstStride,
                                          imageWidth, imageHeight);
                *stride = dstStride;
                *useFloats = false;
                imageConfig = "RGBA_1010102";
                imageData = rgba1010102Data;
                break;
            }
            break;
        case Hardware: {
            if (!loadAHardwareBuffersAPI()) {
                return;
            }
            AHardwareBuffer_Desc bufferDesc = {};
            memset(&bufferDesc, 0, sizeof(AHardwareBuffer_Desc));
            bufferDesc.width = imageWidth;
            bufferDesc.height = imageHeight;
            bufferDesc.layers = 1;
            bufferDesc.format = (*useFloats) ? AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT
                                             : AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
            bufferDesc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

            AHardwareBuffer *hardwareBuffer = nullptr;

            int status = AHardwareBuffer_allocate_compat(&bufferDesc, &hardwareBuffer);
            if (status != 0) {
                return;
            }
            ARect rect = {0, 0, imageWidth, imageHeight};
            uint8_t *buffer;

            status = AHardwareBuffer_lock_compat(hardwareBuffer,
                                                 AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                                                 &rect, reinterpret_cast<void **>(&buffer));
            if (status != 0) {
                AHardwareBuffer_release_compat(hardwareBuffer);
                return;
            }

            AHardwareBuffer_describe_compat(hardwareBuffer, &bufferDesc);

            int pixelSize = (*useFloats) ? sizeof(uint16_t) : sizeof(uint8_t);

            if (*useFloats) {
                int dstStride = imageWidth * 4 * (int) sizeof(uint16_t);
                std::vector<uint8_t> rgbaF16Data(dstStride * imageHeight);
                coder::RgbaF32ToF16(reinterpret_cast<const float *>(imageData.data()),
                                    (int) *stride,
                                    reinterpret_cast<uint16_t *>(rgbaF16Data.data()),
                                    dstStride, imageWidth, imageHeight);
                coder::CopyUnalignedRGBA(rgbaF16Data.data(), dstStride, buffer,
                                         (int) bufferDesc.stride * 4 * pixelSize,
                                         (int) bufferDesc.width,
                                         (int) bufferDesc.height,
                                         (*useFloats) ? sizeof(uint16_t) : sizeof(uint8_t));
            } else {
                coder::CopyUnalignedRGBA(imageData.data(), *stride, buffer,
                                         (int) bufferDesc.stride * 4 * pixelSize,
                                         (int) bufferDesc.width,
                                         (int) bufferDesc.height,
                                         (*useFloats) ? sizeof(uint16_t) : sizeof(uint8_t));
            }

            status = AHardwareBuffer_unlock_compat(hardwareBuffer, nullptr);
            if (status != 0) {
                AHardwareBuffer_release_compat(hardwareBuffer);
                return;
            }

            jobject buf = AHardwareBuffer_toHardwareBuffer_compat(env, hardwareBuffer);

            AHardwareBuffer_release_compat(hardwareBuffer);

            *hwBuffer = buf;
            imageConfig = "HARDWARE";
        }
            break;
        default:
            break;
    }
}