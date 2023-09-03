# JXL Coder for Android 24+

Library provides simple interface to decode or encode ( create ) JXL images for Android
Based on libjxl

ICC profiles supported.

# Usage example

```kotlin
// May decode JXL images, supported RGBA_8888
val bitmap: Bitmap = JxlCoder().decode(buffer) // Decode jxl from ByteArray
val bytes: ByteArray = JxlCoder().encode(decodedBitmap) // Encode Bitmap to JXL
```

# Add Jitpack repository

```groovy
repositories {
    maven { url "https://jitpack.io" }
}
```

```groovy
implementation 'com.github.awxkee:jxlcoder:1.0.5' // or any version above picker from release tags
```

# Self-build

## Requirements

libjxl:

- ndk
- ninja
- cmake
- nasm

**All commands are require the NDK path set by NDK_PATH environment variable**

* If you wish to build for **x86** you have to add a **$INCLUDE_X86** environment variable for
  example:*

```shell
NDK_PATH=/path/to/ndk INCLUDE_X86=yes bash build_jxl.sh
```

# Copyrights

This library created with [`libjxl`](https://github.com/libjxl/libjxl/tree/main) which belongs to JPEG XL Project
Authors which licensed with BSD-3 license