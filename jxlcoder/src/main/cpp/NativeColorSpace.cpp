//
// Created by Radzivon Bartoshyk on 28/01/2026.
//

#include <jni.h>
#include <string>
#include <NativeColorSpace.h>

namespace colorspace {
jobject searchForNamed(JNIEnv *env, std::string &named) {
  jclass bitmapCls = env->FindClass("android/graphics/Bitmap");
  jclass csCls = env->FindClass("android/graphics/ColorSpace");
  jclass csNamedCls = env->FindClass("android/graphics/ColorSpace$Named");

// Get BT2020_PQ enum
  jfieldID pqField = env->GetStaticFieldID(
      csNamedCls, named.c_str(), "Landroid/graphics/ColorSpace$Named;"
  );
  jobject pqEnum = env->GetStaticObjectField(csNamedCls, pqField);

  jmethodID csGet = env->GetStaticMethodID(
      csCls, "get",
      "(Landroid/graphics/ColorSpace$Named;)Landroid/graphics/ColorSpace;"
  );
  jobject pqColorSpace = env->CallStaticObjectMethod(csCls, csGet, pqEnum);
  return pqColorSpace;
}

jobject getJNIColorSpace(JNIEnv *env, NativeColorSpace nativeColorSpace) {
  switch (nativeColorSpace) {
    case Pq2100: {
      std::string name = "BT2020_PQ";
      return searchForNamed(env, name);
      break;
    }
    case Hlg2100: {
      std::string name = "BT2020_HLG";
      return searchForNamed(env, name);
      break;
    }
    case DisplayP3: {
      std::string name = "DISPLAY_P3";
      return searchForNamed(env, name);
      break;
    }
    case DciP3: {
      std::string name = "DCI_P3";
      return searchForNamed(env, name);
      break;
    }
    case LinearSrgb: {
      std::string name = "LINEAR_SRGB";
      return searchForNamed(env, name);
      break;
    }
    case Bt709: {
      std::string name = "BT709";
      return searchForNamed(env, name);
      break;
    }
    case DefaultSrgb: {
      std::string name = "SRGB";
      return searchForNamed(env, name);
      break;
    }
  }
  return nullptr;
}
}