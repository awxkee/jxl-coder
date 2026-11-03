# Weaver

Rust codec implementation used by the Android library. Build the native libraries
with `./build.sh`. To regenerate the C header separately:

```sh
cbindgen --config cbindgen.toml --crate weaver --output include/weaver.h
```

## Encoding options

`JxlEffort` exposes `ULTRA_FAST`, `FASTEST`, `FAST`, `MEDIUM`, `MEDIUM_SLOW`, and
`SLOW`. The default remains `FAST`. ExtraSlow is not exposed or enabled.

## Gainmap encoding and decoding

On Android 14 (API 34) and later, `JxlCoder.encode(bitmap, exif)` automatically
preserves an attached `android.graphics.Gainmap`. The base image, gainmap pixels,
and ISO 21496-1 metadata are written together in a JPEG XL container with a `jhgm`
box. EXIF can be included in the same file.

Single-channel `ALPHA_8` and RGB gainmaps are supported, with the map's original
dimensions. Like the base bitmap, gainmap contents must be CPU-readable software
bitmaps. The gainmap follows the base image's lossy/lossless setting. Android 16
(API 36) HDR-base direction and alternate color primaries are preserved as well.
Older Android versions and bitmaps without a gainmap use the existing encode path.

`JxlCoder.decode` and both `decodeSampled` overloads read the embedded `jhgm`
gainmap, including trailing and Brotli-compressed boxes. On Android 14+ they
return a bitmap with an attached system `Gainmap`; Android chooses the rendition
for the display. Gain samples retain their own dimensions for an unscaled decode
and follow the base image's crop and output size for a sampled decode.

Before Android 14, decoding returns an SDR sRGB bitmap. An SDR base is already
the SDR rendition; an HDR base is converted with the inverse gain in linear
light, using the ISO metadata parsed by Gainforge. Android 14–15 also rebase HDR
images to SDR before attaching the gainmap because the system's HDR-to-SDR
direction API requires Android 16. Android 16+ preserves HDR-base direction.

The base is converted into the primaries used for gain math and tagged with the
matching RGB color space (gamma 2.2) on Android 14+. This also supports alternate
primaries on Android 14–15 without resolving Android 16 APIs. Custom base color
profiles are retained when re-encoding these bitmaps. Gainmap contents use an
RGB 10-bit system bitmap; numerical samples are never color-managed or alpha
premultiplied. Base alpha is preserved separately.

Gainmap decoding and resizing use 16-bit RGBA buffers: full-range `u16` for
integer RGB, or half-float bits in the same storage for floating-point sources
and XYB/ICC output that needs extended range. Color conversion and gain math
use two reusable `f32` scanlines, then overwrite the 16-bit buffer in place.
Each RGBA texture therefore uses 8 bytes per pixel instead of 16; decoding
without resizing also reuses the original pixel allocation. Codec-internal
working memory is unaffected.

Rendering on Android 14 follows the platform's gainmap shader, including its
limitations with nonzero offsets on translucent bases. The regression suite
checks those results against an independently constructed Android gainmap.

Decoding is available on every supported ABI. Encoding remains available on
the ARM64 and ARMv7 Android targets supported by Jixel in this project. Malformed
or unsupported gainmap metadata produces a decoding error.

The decoder is pinned to jxl-rs git revision
`08bb971ff493cbd77642a7f3de3398a2744c4405`. Gainmap box extraction,
decompression, and color-profile decoding use the same `jxl` dependency. The
small version-0 `jhgm` envelope reader is part of Weaver, so a separate
`jxl-gainmap` dependency is unnecessary.

Gainmap regressions run with `cargo test --locked --lib` in this directory and
with `./gradlew :jxlcoder:connectedDebugAndroidTest` from the repository root.
The Android gainmap tests require API 34+; the direction test requires API 36+.
The native tests exercise the pre-34 fallback and API 34/35/36 decisions.
