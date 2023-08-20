# JXL Coder for Android 24+

Library provides simple interface to decode or encode ( create ) JXL images for Android
Based on libjxl

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
implementation 'com.github.awxkee:jxlcoder:1.0.0' // or any version above picker from release tags
```