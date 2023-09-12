//
// Created by Radzivon Bartoshyk on 03/09/2023.
//

#include "colorspace.h"
#include <vector>
#include "icc/lcms2.h"
#include <android/log.h>
#include <thread>
#include "ThreadPool.hpp"

void convertUseDefinedColorSpace(std::vector<uint8_t> &vector, int stride, int width, int height,
                                 const unsigned char *colorSpace, size_t colorSpaceSize,
                                 bool image16Bits) {
    cmsContext context = cmsCreateContext(nullptr, nullptr);
    std::shared_ptr<void> contextPtr(context, [](void *profile) {
        cmsDeleteContext(reinterpret_cast<cmsContext>(profile));
    });
    cmsHPROFILE srcProfile = cmsOpenProfileFromMem(colorSpace, colorSpaceSize);
    if (!srcProfile) {
        // JUST RETURN without signalling error, better proceed with invalid photo than crash
        __android_log_print(ANDROID_LOG_ERROR, "JXLCoder", "ColorProfile Allocation Failed");
        return;
    }
    std::shared_ptr<void> ptrSrcProfile(srcProfile, [](void *profile) {
        cmsCloseProfile(reinterpret_cast<cmsHPROFILE>(profile));
    });
    cmsHPROFILE dstProfile = cmsCreate_sRGBProfileTHR(
            reinterpret_cast<cmsContext>(contextPtr.get()));
    std::shared_ptr<void> ptrDstProfile(dstProfile, [](void *profile) {
        cmsCloseProfile(reinterpret_cast<cmsHPROFILE>(profile));
    });
    cmsHTRANSFORM transform = cmsCreateTransform(ptrSrcProfile.get(),
                                                 image16Bits ? TYPE_RGBA_FLT : TYPE_RGBA_8,
                                                 ptrDstProfile.get(),
                                                 image16Bits ? TYPE_RGBA_FLT : TYPE_RGBA_8,
                                                 INTENT_PERCEPTUAL,
                                                 cmsFLAGS_BLACKPOINTCOMPENSATION |
                                                 cmsFLAGS_NOWHITEONWHITEFIXUP |
                                                 cmsFLAGS_COPY_ALPHA);
    if (!transform) {
        // JUST RETURN without signalling error, better proceed with invalid photo than crash
        __android_log_print(ANDROID_LOG_ERROR, "AVIFCoder", "ColorProfile Creation has hailed");
        return;
    }
    std::shared_ptr<void> ptrTransform(transform, [](void *transform) {
        cmsDeleteTransform(reinterpret_cast<cmsHTRANSFORM>(transform));
    });

    ThreadPool pool;
    std::vector<std::future<void>> results;

    std::vector<char> iccARGB;
    iccARGB.resize(stride * height);
    for (int y = 0; y < height; ++y) {
        auto r = pool.enqueue(cmsDoTransformLineStride, ptrTransform.get(),
                           vector.data() + stride * y, iccARGB.data() + stride * y, width, 1,
                           stride, stride, 0, 0);
        results.push_back(std::move(r));
    }

    for (auto &result: results) {
        result.wait();
    }
    std::copy(iccARGB.begin(), iccARGB.end(), vector.begin());
}
