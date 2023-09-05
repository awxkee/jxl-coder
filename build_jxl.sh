#!/bin/bash
set -e

export NDK=$NDK_PATH

destination_directory=libjxl
if [ ! -d "$destination_directory" ]; then
    git clone https://github.com/libjxl/libjxl
else
    echo "Destination directory '$destination_directory' already exists. Cloning skipped."
fi

rm -rf ./libjxl/deps.sh
cp deps.sh ./libjxl/deps.sh

if [ -z "$INCLUDE_X86" ]; then
  ABI_LIST="armeabi-v7a arm64-v8a x86_64"
  echo "X86 won't be included into a build"
else
  ABI_LIST="armeabi-v7a arm64-v8a x86 x86_64"
fi

cd libjxl

bash deps.sh

export SJPEG_ANDROID_NDK_PATH=$NDK_PATH

for abi in ${ABI_LIST}; do
  rm -rf "build-${abi}"
  mkdir "build-${abi}"
  cd "build-${abi}"

  cmake .. \
    -G "Ninja" \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=${abi} -DCMAKE_ANDROID_ARCH_ABI=${abi} \
    -DANDROID_NDK=${NDK} \
    -DSJPEG_ANDROID_NDK_PATH=${NDK} \
    -DANDROID_PLATFORM=android-21 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_SYSTEM_NAME=Generic \
    -DCMAKE_ANDROID_STL_TYPE=c++_shared \
    -DCMAKE_SYSTEM_NAME=Android \
    -DTHREADS_PREFER_PTHREAD_FLAG=TRUE \
    -DJPEGXL_ENABLE_TOOLS=FALSE \
    -DBUILD_TESTING=OFF \
    -DJPEGXL_ENABLE_MANPAGES=FALSE \
    -DJPEGXL_ENABLE_JPEGLI_LIBJPEG=FALSE \
    -DJPEGXL_ENABLE_EXAMPLES=OFF \
    -DANDROID=TRUE
  ninja
  cd ..
done

for abi in ${ABI_LIST}; do
  mkdir -p "../jxlcoder/src/main/cpp/lib/${abi}"
  cp -r "build-${abi}/*" "../jxlcoder/src/main/cpp/lib/${abi}/"
  echo "build-${abi}/* was successfully copied to ../jxlcoder/src/main/cpp/lib/${abi}/"
done
