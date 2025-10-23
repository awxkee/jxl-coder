#!/bin/bash
set -e

export NDK=$NDK_PATH

destination_directory=libjxl
if [ ! -d "$destination_directory" ]; then
    git clone https://github.com/libjxl/libjxl
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"

cd libjxl

bash deps.sh

export SJPEG_ANDROID_NDK_PATH=$NDK_PATH

for abi in ${ABI_LIST}; do
  rm -rf "build-${abi}"
  mkdir "build-${abi}"
  cd "build-${abi}"

  echo $ARCH_OPTIONS

  if [ "$abi" == "arm64-v8a" ]; then
    cmake .. \
      -G "Ninja" \
      -Wno-dev -Wno-policy \
      -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=${abi} -DCMAKE_ANDROID_ARCH_ABI=${abi} \
      -DANDROID_NDK=${NDK} \
      -DSJPEG_ANDROID_NDK_PATH=${NDK} \
      -DANDROID_PLATFORM=android-21 \
      -DCMAKE_C_FLAGS="-DJXL_HIGH_PRECISION=0" \
      -DCMAKE_CXX_FLAGS="-DJXL_HIGH_PRECISION=0" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_SYSTEM_NAME=Generic \
      -DCMAKE_ANDROID_STL_TYPE=c++_shared \
      -DCMAKE_SYSTEM_NAME=Android \
      -DCMAKE_THREAD_PREFER_PTHREAD=TRUE \
      -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-z,max-page-size=16384" \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DTHREADS_PREFER_PTHREAD_FLAG=TRUE \
      -DJPEGXL_ENABLE_TOOLS=OFF \
      -DBUILD_TESTING=OFF \
      -DJPEGXL_ENABLE_SKCMS=true \
      -DJPEGXL_ENABLE_JPEGLI_LIBJPEG=OFF \
      -DJPEGXL_ENABLE_JPEGLI=OFF \
      -DJPEGXL_ENABLE_SJPEG=OFF \
      -DJPEGXL_ENABLE_MANPAGES=FALSE \
      -DJPEGXL_ENABLE_JPEGLI_LIBJPEG=FALSE \
      -DENABLE_SKCMS_DEFAULT=FALSE \
      -DJPEGXL_ENABLE_EXAMPLES=OFF \
      -DJPEGXL_ENABLE_HWY_SVE=false \
      -DJPEGXL_ENABLE_HWY_SVE2=false \
      -DJPEGXL_ENABLE_HWY_SVE_256=false \
      -DJPEGXL_ENABLE_HWY_SVE2_128=false \
      -DJPEGXL_ENABLE_HWY_NEON_BF16=false \
      -DJPEGXL_ENABLE_HWY_NEON_WITHOUT_AES=true \
      -DJPEGXL_ENABLE_HWY_NEON=false \
      -DJPEGXL_ENABLE_HWY_SCALAR=false \
      -DJPEGXL_ENABLE_HWY_EMU128=false \
      -DJPEGXL_ENABLE_HWY_AVX3=false \
      -DJPEGXL_ENABLE_HWY_AVX3_DL=false \
      -DJPEGXL_ENABLE_HWY_SSE2=false \
      -DJPEGXL_ENABLE_HWY_SSE4=false \
      -DJPEGXL_ENABLE_HWY_SSSE3=false \
      -DJPEGXL_ENABLE_HWY_AVX2=false \
      -DJPEGXL_BUNDLE_LIBPNG=false \
      -DANDROID=TRUE
  elif [[ "$abi" == "x86_64" || "$abi" == "x86" ]]; then
    cmake .. \
      -G "Ninja" \
      -Wno-dev -Wno-policy \
      -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=${abi} -DCMAKE_ANDROID_ARCH_ABI=${abi} \
      -DANDROID_NDK=${NDK} \
      -DSJPEG_ANDROID_NDK_PATH=${NDK} \
      -DCMAKE_C_FLAGS="-DJXL_HIGH_PRECISION=0" \
      -DCMAKE_CXX_FLAGS="-DJXL_HIGH_PRECISION=0" \
      -DANDROID_PLATFORM=android-21 \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_SYSTEM_NAME=Generic \
      -DCMAKE_ANDROID_STL_TYPE=c++_shared \
      -DCMAKE_SYSTEM_NAME=Android \
      -DCMAKE_THREAD_PREFER_PTHREAD=TRUE \
      -DJPEGXL_ENABLE_SKCMS=true \
      -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-z,max-page-size=16384" \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DTHREADS_PREFER_PTHREAD_FLAG=TRUE \
      -DJPEGXL_ENABLE_TOOLS=OFF \
      -DBUILD_TESTING=OFF \
      -DJPEGXL_ENABLE_JPEGLI_LIBJPEG=OFF \
      -DJPEGXL_ENABLE_JPEGLI=OFF \
      -DJPEGXL_ENABLE_SJPEG=OFF \
      -DJPEGXL_ENABLE_MANPAGES=FALSE \
      -DJPEGXL_ENABLE_JPEGLI_LIBJPEG=FALSE \
      -DENABLE_SKCMS_DEFAULT=FALSE \
      -DJPEGXL_ENABLE_EXAMPLES=OFF \
      -DJPEGXL_ENABLE_HWY_SCALAR=false \
      -DJPEGXL_ENABLE_HWY_EMU128=false \
      -DJPEGXL_ENABLE_HWY_AVX3=false \
      -DJPEGXL_ENABLE_HWY_AVX3_DL=false \
      -DJPEGXL_ENABLE_HWY_SSE2=true \
      -DJPEGXL_ENABLE_HWY_SSE4=false \
      -DJPEGXL_ENABLE_HWY_SSSE3=false \
      -DJPEGXL_ENABLE_HWY_AVX2=false \
      -DJPEGXL_BUNDLE_LIBPNG=false \
      -DANDROID=TRUE
  else
    cmake .. \
      -G "Ninja" \
      -Wno-dev -Wno-policy \
      -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=${abi} -DCMAKE_ANDROID_ARCH_ABI=${abi} \
      -DANDROID_NDK=${NDK} \
      -DCMAKE_C_FLAGS="-DJXL_HIGH_PRECISION=0" \
      -DCMAKE_CXX_FLAGS="-DJXL_HIGH_PRECISION=0" \
      -DSJPEG_ANDROID_NDK_PATH=${NDK} \
      -DANDROID_PLATFORM=android-21 \
      -DCMAKE_BUILD_TYPE=Release \
      -DJPEGXL_ENABLE_SKCMS=true \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_SYSTEM_NAME=Generic \
      -DCMAKE_ANDROID_STL_TYPE=c++_shared \
      -DCMAKE_SYSTEM_NAME=Android \
      -DCMAKE_THREAD_PREFER_PTHREAD=TRUE \
      -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-z,max-page-size=16384" \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DTHREADS_PREFER_PTHREAD_FLAG=TRUE \
      -DJPEGXL_ENABLE_TOOLS=OFF \
      -DBUILD_TESTING=OFF \
      -DJPEGXL_ENABLE_JPEGLI_LIBJPEG=OFF \
      -DJPEGXL_ENABLE_JPEGLI=OFF \
      -DJPEGXL_ENABLE_SJPEG=OFF \
      -DJPEGXL_ENABLE_MANPAGES=FALSE \
      -DJPEGXL_ENABLE_JPEGLI_LIBJPEG=FALSE \
      -DENABLE_SKCMS_DEFAULT=FALSE \
      -DJPEGXL_ENABLE_EXAMPLES=OFF \
      -DJPEGXL_ENABLE_HWY_SCALAR=false \
      -DJPEGXL_ENABLE_HWY_EMU128=true \
      -DJPEGXL_BUNDLE_LIBPNG=false \
      -DANDROID=TRUE
  fi

  ninja
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip lib/libjxl_threads.so
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip lib/libjxl_cms.so
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip lib/libjxl.so
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip third_party/brotli/libbrotlicommon.so
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip third_party/brotli/libbrotlidec.so
  $NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip third_party/brotli/libbrotlienc.so
  cd ..
done

for abi in ${ABI_LIST}; do
  mkdir -p "../jxlcoder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/lib/libjxl_cms.so" "../jxlcoder/src/main/cpp/lib/${abi}/libjxl_cms.so"
  cp -r "build-${abi}/lib/libjxl.so" "../jxlcoder/src/main/cpp/lib/${abi}/libjxl.so"
  cp -r "build-${abi}/lib/libjxl_threads.so" "../jxlcoder/src/main/cpp/lib/${abi}/libjxl_threads.so"
  cp -r "build-${abi}/third_party/brotli/libbrotlicommon.so" "../jxlcoder/src/main/cpp/lib/${abi}/libbrotlicommon.so"
  cp -r "build-${abi}/third_party/brotli/libbrotlidec.so" "../jxlcoder/src/main/cpp/lib/${abi}/libbrotlidec.so"
  cp -r "build-${abi}/third_party/brotli/libbrotlienc.so" "../jxlcoder/src/main/cpp/lib/${abi}/libbrotlienc.so"
  echo "build-${abi}/* was successfully copied to ../jxlcoder/src/main/cpp/lib/${abi}/"
done
