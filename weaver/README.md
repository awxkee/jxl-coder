# Weaver

Rust codec implementation used by the Android library. Build the native libraries
with `./build.sh`. To regenerate the C header separately:

```sh
cbindgen --config cbindgen.toml --crate weaver --output include/weaver.h
```

## Encoding options

`JxlEffort` exposes `ULTRA_FAST`, `FASTEST`, `FAST`, `MEDIUM`, `MEDIUM_SLOW`, and
`SLOW`. The default remains `FAST`. ExtraSlow is not exposed or enabled.

Splines and dots are separate, optional tools, both disabled by default:

```kotlin
val encoded = JxlCoder.encode(
    bitmap,
    exif = null,
    effort = JxlEffort.SLOW,
    splines = true,
    dots = true,
)
```

Jixel uses splines for thin curves and dots for isolated bright or dark spots.
Both tools are experimental and apply only to lossy encoding at `SLOW` effort;
other effort levels and lossless encoding ignore these switches.

## Gainmap encoding

On Android 14 (API 34) and later, `JxlCoder.encode(bitmap, exif)` automatically
preserves an attached `android.graphics.Gainmap`. The base image, gainmap pixels,
and ISO 21496-1 metadata are written together in a JPEG XL container with a `jhgm`
box. EXIF can be included in the same file.

Single-channel `ALPHA_8` and RGB gainmaps are supported, with the map's original
dimensions. Like the base bitmap, gainmap contents must be CPU-readable software
bitmaps. The gainmap follows the base image's lossy/lossless setting. Android 16
(API 36) HDR-base direction and alternate color primaries are preserved as well.
Older Android versions and bitmaps without a gainmap use the existing encode path.

This adds encoding only. `JxlCoder.decode` continues to return the base rendition;
it does not restore or apply the embedded gainmap. Encoding remains available on
the ARM64 and ARMv7 Android targets supported by Jixel in this project.
