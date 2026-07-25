# JXL (JPEG XL) Coder

Supports version from Android 5.0 (API Level 21)

The JPEG XL Codec for Android is a versatile and efficient library that allows you to decode and
encode JPEG XL (JXL) images within your Android applications. With this library, you can seamlessly
integrate support for the next-generation image format, providing both decoding and encoding
capabilities to enhance your app's image handling capabilities. Simple and convenient interface for kotlin

ICC profiles supported. Supports animations as well

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

## Construct from JPEG
```kotlin
// Construct JPEG XL from JPEG data
 val jxlData = JxlCoder.transcode(jpegByteArray)
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

# Add to project

```groovy
implementation 'io.github.awxkee:jxl-coder:2.2.0' // or any version above picker from release tags

// Glide JPEG XL plugin if you need one
implementation 'io.github.awxkee:jxl-coder-glide:2.2.0' // or any version above picker from release tags

// Coil JPEG XL plugin if you need one
implementation 'io.github.awxkee:jxl-coder-coil:2.2.0' // or any version above picker from release tags
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

This library created with [`jixel`](https://github.com/awxkee/jixel) and ['jxl-rs'](https://github.com/libjxl/jxl-rs) which belongs to
JPEG XL Project
Authors which licensed with BSD-3 license