cmake_minimum_required(VERSION 3.22.1)

project("rjxlcoder")

add_library(libweaver STATIC IMPORTED)

add_library(${CMAKE_PROJECT_NAME} SHARED
        JxlEncoder.cpp
        JniDecoding.cpp
        Support.cpp
        JxlAnimatedDecoderCoordinator.cpp
        JniExceptions.cpp
)

set_target_properties(rjxlcoder libweaver PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/lib/${ANDROID_ABI}/libweaver.a)

target_include_directories("rjxlcoder" PRIVATE ${CMAKE_SOURCE_DIR}/jxl ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/libyuv ${CMAKE_SOURCE_DIR}/algo)

add_library(cpufeatures STATIC ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)
target_include_directories(cpufeatures PUBLIC ${ANDROID_NDK}/sources/android/cpufeatures)
target_link_libraries(cpufeatures dl)
list(APPEND EXTRA_LIBS cpufeatures)

target_link_options(rjxlcoder PRIVATE "-Wl,-z,max-page-size=16384")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckCXXCompilerFlag)
if (ANDROID_ABI STREQUAL arm64-v8a)
    add_definitions("-DHAVE_NEON=1")
endif ()

if (ANDROID_ABI STREQUAL x86-64)
    set_target_properties(rjxlcoder PROPERTIES COMPILE_FLAGS "-march=i686")
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffast-math -funroll-loops")
endif ()

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        libweaver
        android
        cpufeatures
        -ljnigraphics
        z
        log)