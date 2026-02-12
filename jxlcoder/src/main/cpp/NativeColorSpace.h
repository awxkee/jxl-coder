//
// Created by Radzivon Bartoshyk on 28/01/2026.
//

#ifndef JXLCODER_JXLCODER_SRC_MAIN_CPP_NATIVECOLORSPACE_H_
#define JXLCODER_JXLCODER_SRC_MAIN_CPP_NATIVECOLORSPACE_H_

#include <jni.h>

enum NativeColorSpace {
  Pq2100,
  Hlg2100,
  DisplayP3,
  DciP3,
  LinearSrgb,
  Bt709,
  DefaultSrgb
};

namespace colorspace {
jobject getJNIColorSpace(JNIEnv *env, NativeColorSpace nativeColorSpace);
}

#endif //JXLCODER_JXLCODER_SRC_MAIN_CPP_NATIVECOLORSPACE_H_
