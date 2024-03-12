# JXL (JPEG XL) Coder

Supports version from Android 5.0 (API Level 21)

The JPEG XL Codec for Android is a versatile and efficient library that allows you to decode and
encode JPEG XL (JXL) images within your Android applications. With this library, you can seamlessly
integrate support for the next-generation image format, providing both decoding and encoding
capabilities to enhance your app's image handling capabilities. Simple and convenient interface for kotlin

ICC profiles supported. Supports animations as well

Image processing speeded up by [libhwy](https://github.com/google/highway)

# Usage example

```kotlin
// May decode JXL images, supported RGBA_8888, RGBA_F16, RGBA_1010102, RGB_565, HARDWARE
val bitmap: Bitmap = JxlCoder.decode(buffer) // Decode JPEG XL from ByteArray
// If you need a sample
val bitmap: Bitmap =
    JxlCoder.decodeSampled(buffer, width, height) // Decode JPEG XL from ByteArray with given size
val bytes: ByteArray = JxlCoder.encode(decodedBitmap) // Encode Bitmap to JPEG XL
```

# Convenience for conversion

## Construct from JPEG or Reconstruct JPEG from JXL
```kotlin
// Construct JPEG XL from JPEG data
 val jxlData = JxlCoder.Convenience.construct(jpegByteArray)
// Re-construct JPEG from JXL data
 val jpegData = JxlCoder.Convenience.reconstructJPEG(jxlByteArray)
```

## Create JPEG XL from GIF
```kotlin
// Construct animated JPEG XL from GIF data
 val jxlData = JxlCoder.Convenience.gif2JXL(gifByteArray)
```

## Create JPEG XL from APNG
```kotlin
// Construct animated JPEG XL from APNG data
 val jxlData = JxlCoder.Convenience.apng2JXL(gifByteArray)
```

# Animation Decoding

```kotlin
val animatedImage = JxlAnimatedImage(jxlBuffer) // Creates an animated image
val frames = numberOfFrames
val drawable =
    animatedImage.animatedDrawable // if you just wish get an animated drawable NOT OPTIMIZED It will just render all bitmaps into one drawable
for (frame in 0 until frames) {
    val frameDuration = getFrameDuration(frame)
    val frameBitmap = getFrame(frame)
    // Do something with frame and duration
}
```

# Animation Encoding

```kotlin
val encoder = JxlAnimatedEncoder(
    width = width,
    height = width,
)
encoder.addFrame(firstFrame, duration = 2000) // Duration in ms
encoder.addFrame(secondFrame, duration = 2000) // Duration in ms
val compressedBuffer: ByteArray = encoder.encode() // Do something with buffer
```

# Add Jitpack repository

```groovy
repositories {
    maven { url "https://jitpack.io" }
}
```

```groovy
implementation 'com.github.awxkee:jxl-coder:2.1.5' // or any version above picker from release tags

// Glide JPEG XL plugin if you need one
implementation 'com.github.awxkee:jxl-coder-glide:2.1.5' // or any version above picker from release tags

// Coil JPEG XL plugin if you need one
implementation 'com.github.awxkee:jxl-coder-coil:2.1.5' // or any version above picker from release tags
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

This library created with [`libjxl`](https://github.com/libjxl/libjxl/tree/main) which belongs to
JPEG XL Project
Authors which licensed with BSD-3 license

# Disclaimer

The JPEG XL call for proposals talks about the requirement of a next generation image compression
standard with substantially better compression efficiency (60% improvement) comparing to JPEG. The
standard is expected to outperform the still image compression performance shown by HEIC, AVIF,
WebP, and JPEG 2000. It also provides efficient lossless recompression options for images in the
traditional/legacy JPEG format.

JPEG XL supports lossy compression and lossless compression of ultra-high-resolution images (up to 1
terapixel), up to 32 bits per component, up to 4099 components (including alpha transparency),
animated images, and embedded previews. It has features aimed at web delivery such as advanced
progressive decoding[13] and minimal header overhead, as well as features aimed at image editing and
digital printing, such as support for multiple layers, CMYK, and spot colors. It is specifically
designed to seamlessly handle wide color gamut color spaces with high dynamic range such as Rec.
2100 with the PQ or HLG transfer function. 
