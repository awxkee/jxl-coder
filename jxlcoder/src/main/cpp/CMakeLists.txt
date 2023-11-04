# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("jxlcoder")

add_library(libbrotlicommon SHARED IMPORTED)
add_library(libbrotlidec SHARED IMPORTED)
add_library(libbrotlienc SHARED IMPORTED)
add_library(libjxl SHARED IMPORTED)
add_library(libjxl_threads SHARED IMPORTED)
add_library(libyuv STATIC IMPORTED)
add_library(libhwy STATIC IMPORTED)

add_library(${CMAKE_PROJECT_NAME} SHARED
        JxlEncoder.cpp icc/cmsalpha.c icc/cmscam02.c icc/cmscgats.c icc/cmscnvrt.c icc/cmserr.c icc/cmsgamma.c
        icc/cmsgmt.c icc/cmshalf.c icc/cmsintrp.c icc/cmsio0.c icc/cmsio1.c icc/cmslut.c icc/cmsmd5.c icc/cmsmtrx.c icc/cmsnamed.c
        icc/cmsopt.c icc/cmspack.c icc/cmspcs.c icc/cmsplugin.c icc/cmsps2.c icc/cmssamp.c icc/cmssm.c icc/cmstypes.c icc/cmsvirt.c
        icc/cmswtpnt.c icc/cmsxform.c colorspace.cpp stb_image_resize.h HalfFloats.cpp JniExceptions.cpp JxlEncoding.cpp
        JxlDecoding.cpp JniDecoding.cpp Rgba2Rgb.cpp Rgb1010102toF16.cpp Rgba16bitCopy.cpp
        F32ToRGB1010102.cpp Rgba1010102toF32.cpp HardwareBuffersCompat.cpp SizeScaler.cpp
        Support.cpp ReformatBitmap.cpp Rgb565.cpp Rgb1010102.cpp F32toU8.cpp Rgba8ToF16.cpp CopyUnaligned.cpp
        XScaler.cpp RgbaF16bitNBitU8.cpp)

target_include_directories("jxlcoder" PRIVATE ${CMAKE_SOURCE_DIR}/jxl ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libyuv)

set_target_properties(jxlcoder libbrotlicommon PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libbrotlicommon.so)
set_target_properties(jxlcoder libbrotlidec PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libbrotlidec.so)
set_target_properties(jxlcoder libbrotlienc PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libbrotlienc.so)
set_target_properties(jxlcoder libjxl PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libjxl.so)
set_target_properties(jxlcoder libjxl_threads PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libjxl_threads.so)
set_target_properties(jxlcoder libyuv PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libyuv.a)
set_target_properties(jxlcoder libhwy PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libhwy.a)

add_library(cpufeatures STATIC ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)
target_include_directories(cpufeatures PUBLIC ${ANDROID_NDK}/sources/android/cpufeatures)
target_link_libraries(cpufeatures dl)
list(APPEND EXTRA_LIBS cpufeatures)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckCXXCompilerFlag)

if (ANDROID_ABI STREQUAL x86)
    target_compile_options(jxlcoder PRIVATE "-msse2")

    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
    endif()
endif ()

if (ANDROID_ABI STREQUAL x86-64)
    set_target_properties(jxlcoder PROPERTIES COMPILE_FLAGS "-march=i686")

    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
    endif()
endif ()


add_definitions(-DSTB_IMAGE_RESIZE_IMPLEMENTATION -DCMS_NO_REGISTER_KEYWORD -DHWY_COMPILE_ONLY_STATIC)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffast-math")
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DHWY_COMPILE_ONLY_STATIC)
endif ()

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        android
        cpufeatures
        libbrotlicommon
        libbrotlidec
        libbrotlienc
        libhwy
        libjxl
        libjxl_threads
        -ljnigraphics
        libyuv
        log)