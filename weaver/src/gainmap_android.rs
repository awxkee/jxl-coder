/*
 * Copyright (c) Radzivon Bartoshyk. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3.  Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
//! Preserve an Android Bitmap's attached gainmap when encoding JPEG XL.

#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
use crate::cvt::{ar30_bytes_to_rgba10, f16_bytes_to_rgba10, rgb565_bytes_to_rgba8888};
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
use crate::ffi::{get_bitmap_data, BitmapPixelFormat};
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
use crate::gainmap::GainMapParameters;
use crate::gainmap_decode::GainMetadata;
use crate::jxl_decode::DecodedJxlPacket;
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
use anyhow::ensure;
use anyhow::Result;
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
use jixel::GainMap;
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
use jni::objects::JFloatArray;
use jni::objects::{JObject, JValue};
use jni::strings::JNIString;
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
use jni::sys::jobject;
use jni::{jni_sig, jni_str, Env};
use moxcms::ColorProfile;
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
use moxcms::{Chromaticity, ColorPrimaries, ToneReprCurve, XyY};

/// Called only on API 34+. No Gainmap classes or methods are resolved on old OSes.
pub(crate) fn gainmap_color_space<'local>(
    env: &mut Env<'local>,
    profile: &ColorProfile,
) -> Result<JObject<'local>> {
    // Match the gamut used for gain math. Converting the base to sRGB before
    // attaching a gainmap with per-channel gains changes the HDR rendition.
    let matrix = profile.rgb_to_xyz_matrix();
    let values: Vec<f32> = (0..3)
        .flat_map(|c| (0..3).map(move |r| matrix.v[r][c] as f32))
        .collect();
    let transform = env.new_float_array(9)?;
    transform.set_region(env, 0, &values)?;
    let name = env.new_string("JPEG XL gainmap RGB")?;
    Ok(env.new_object(
        jni_str!("android/graphics/ColorSpace$Rgb"),
        jni_sig!("(Ljava/lang/String;[FD)V"),
        &[
            JValue::Object(&name),
            JValue::Object(&transform),
            JValue::Double(2.2),
        ],
    )?)
}

pub(crate) fn attach_gainmap(
    env: &mut Env,
    bitmap: &JObject,
    image: &DecodedJxlPacket<u16>,
    metadata: GainMetadata,
    sdk: i32,
) -> Result<()> {
    use crate::ffi::software_bitmap;
    use crate::support::PackedImageBuffer;
    use crate::WeaverPreferredColorConfig;

    let data = image
        .data
        .as_chunks::<4>()
        .0
        .iter()
        .flat_map(|p| {
            // Retain 10-bit numerical precision in a system-supported gainmap bitmap.
            let rgb = |c: usize| (u32::from(p[c]) * 1023 + 32767) / 65535;
            (rgb(0) | (rgb(1) << 10) | (rgb(2) << 20) | (3 << 30)).to_ne_bytes()
        })
        .collect();
    let contents = software_bitmap(
        env,
        &PackedImageBuffer {
            data,
            width: image.width,
            height: image.height,
            format: WeaverPreferredColorConfig::Rgba1010102,
        },
        None,
    )?;
    let gainmap = env.new_object(
        jni_str!("android/graphics/Gainmap"),
        jni_sig!("(Landroid/graphics/Bitmap;)V"),
        &[JValue::Object(&contents)],
    )?;
    let hdr_base = metadata.hdr_base();
    let (sdr_offset, hdr_offset, sdr_headroom, hdr_headroom) = if hdr_base {
        (
            metadata.alternate_offset,
            metadata.base_offset,
            metadata.alternate_headroom,
            metadata.base_headroom,
        )
    } else {
        (
            metadata.base_offset,
            metadata.alternate_offset,
            metadata.base_headroom,
            metadata.alternate_headroom,
        )
    };
    for (method, values) in [
        ("setRatioMin", metadata.min.map(f32::exp2)),
        ("setRatioMax", metadata.max.map(f32::exp2)),
        ("setGamma", metadata.gamma.map(f32::recip)),
        ("setEpsilonSdr", sdr_offset),
        ("setEpsilonHdr", hdr_offset),
    ] {
        env.call_method(
            &gainmap,
            JNIString::from(method),
            jni_sig!("(FFF)V"),
            &[
                JValue::Float(values[0]),
                JValue::Float(values[1]),
                JValue::Float(values[2]),
            ],
        )?;
    }
    for (method, value) in [
        ("setMinDisplayRatioForHdrTransition", sdr_headroom.exp2()),
        ("setDisplayRatioForFullHdr", hdr_headroom.exp2()),
    ] {
        env.call_method(
            &gainmap,
            JNIString::from(method),
            jni_sig!("(F)V"),
            &[JValue::Float(value)],
        )?;
    }
    if sdk >= 36 {
        env.call_method(
            &gainmap,
            jni_str!("setGainmapDirection"),
            jni_sig!("(I)V"),
            &[JValue::Int(i32::from(hdr_base))],
        )?;
    }
    env.call_method(
        bitmap,
        jni_str!("setGainmap"),
        jni_sig!("(Landroid/graphics/Gainmap;)V"),
        &[JValue::Object(&gainmap)],
    )?;
    Ok(())
}

#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
fn float_array<const N: usize>(env: &mut Env, object: &JObject, method: &str) -> Result<[f32; N]> {
    let object = env
        .call_method(object, JNIString::from(method), jni_sig!("()[F"), &[])?
        .l()?;
    // SAFETY: the method's return type is float[], and into_raw transfers ownership.
    let array = unsafe { JFloatArray::from_raw(env, object.into_raw().cast()) };
    ensure!(
        array.len(env)? == N,
        "Invalid gainmap {method} array length"
    );
    let mut values = [0.0; N];
    array.get_region(env, 0, &mut values)?;
    Ok(values)
}

#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
fn float(env: &mut Env, object: &JObject, method: &str) -> Result<f32> {
    Ok(env
        .call_method(object, JNIString::from(method), jni_sig!("()F"), &[])?
        .f()?)
}

// Android only uses this ColorSpace's primaries and white point for gainmap
// application. A linear ICC profile preserves even custom RGB chromaticities.
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
fn alternate_icc(env: &mut Env, color_space: &JObject) -> Result<Vec<u8>> {
    let primaries = float_array::<6>(env, color_space, "getPrimaries")?;
    let white = float_array::<2>(env, color_space, "getWhitePoint")?;
    ensure!(
        primaries.iter().chain(white.iter()).all(|v| v.is_finite()),
        "Invalid gainmap color primaries"
    );
    ensure!(
        white[1] > 0.0 && primaries[1] != 0.0 && primaries[3] != 0.0 && primaries[5] != 0.0,
        "Invalid gainmap chromaticities"
    );
    let chromaticity = |i: usize| Chromaticity {
        x: primaries[i],
        y: primaries[i + 1],
    };
    let mut profile = ColorProfile::new_srgb();
    profile.update_rgb_colorimetry(
        XyY::new(white[0] as f64, white[1] as f64, 1.0),
        ColorPrimaries {
            red: chromaticity(0),
            green: chromaticity(2),
            blue: chromaticity(4),
        },
    );
    let linear = ToneReprCurve::Parametric(vec![1.0]);
    profile.red_trc = Some(linear.clone());
    profile.green_trc = Some(linear.clone());
    profile.blue_trc = Some(linear);
    Ok(profile.encode()?)
}

/// Preserve custom bitmap gamuts as well as named Android color spaces. A
/// gainmap's per-channel ratios are only meaningful with the matching base ICC.
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
pub(crate) fn read_bitmap_icc(env: &mut Env, image: jobject) -> Result<Option<Vec<u8>>> {
    if crate::support::android_os_version() < 26 {
        return Ok(None);
    }
    let bitmap = unsafe { JObject::from_raw(env, image) };
    let cs = env
        .call_method(
            &bitmap,
            jni_str!("getColorSpace"),
            jni_sig!("()Landroid/graphics/ColorSpace;"),
            &[],
        )?
        .l()?;
    if cs.is_null() {
        return Ok(None);
    }
    ensure!(
        env.is_instance_of(&cs, jni_str!("android/graphics/ColorSpace$Rgb"))?,
        "JPEG XL encoding requires an RGB bitmap color space"
    );
    let mut profile = ColorProfile::new_from_slice(&alternate_icc(env, &cs)?)?;
    let parameters = env
        .call_method(
            &cs,
            jni_str!("getTransferParameters"),
            jni_sig!("()Landroid/graphics/ColorSpace$Rgb$TransferParameters;"),
            &[],
        )?
        .l()?;
    let trc = if !parameters.is_null() {
        let mut values = Vec::new();
        for field in ["g", "a", "b", "c", "d", "e", "f"] {
            values.push(
                env.get_field(&parameters, JNIString::from(field), jni_sig!("D"))?
                    .d()? as f32,
            );
        }
        ToneReprCurve::Parametric(values)
    } else {
        // PQ, HLG and user-defined EOTFs need a sampled transfer curve.
        let eotf = env
            .call_method(
                &cs,
                jni_str!("getEotf"),
                jni_sig!("()Ljava/util/function/DoubleUnaryOperator;"),
                &[],
            )?
            .l()?;
        let mut values = Vec::with_capacity(4097);
        for i in 0..=4096 {
            let v = env
                .call_method(
                    &eotf,
                    jni_str!("applyAsDouble"),
                    jni_sig!("(D)D"),
                    &[JValue::Double(i as f64 / 4096.0)],
                )?
                .d()?;
            ensure!(v.is_finite(), "Invalid bitmap transfer function");
            values.push((v.clamp(0.0, 1.0) * 65535.0).round() as u16);
        }
        ToneReprCurve::Lut(values)
    };
    profile.red_trc = Some(trc.clone());
    profile.green_trc = Some(trc.clone());
    profile.blue_trc = Some(trc);
    Ok(Some(profile.encode()?))
}

#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
pub(crate) fn read_gain_map(
    env: &mut Env,
    image: jobject,
    lossless: bool,
) -> Result<Option<GainMap>> {
    let sdk = env
        .get_static_field(
            jni_str!("android/os/Build$VERSION"),
            jni_str!("SDK_INT"),
            jni_sig!("I"),
        )?
        .i()?;
    // Do not resolve any API 34 methods on older devices.
    if sdk < 34 {
        return Ok(None);
    }
    // SAFETY: image is the Bitmap reference passed to the encoder's JNI entry point.
    let bitmap = unsafe { JObject::from_raw(env, image) };
    if !env
        .call_method(&bitmap, jni_str!("hasGainmap"), jni_sig!("()Z"), &[])?
        .z()?
    {
        return Ok(None);
    }
    let gainmap = env
        .call_method(
            &bitmap,
            jni_str!("getGainmap"),
            jni_sig!("()Landroid/graphics/Gainmap;"),
            &[],
        )?
        .l()?;
    ensure!(
        !gainmap.is_null(),
        "Bitmap reports a gainmap but returned null"
    );

    let mut hdr_base = false;
    let mut icc = None;
    if sdk >= 36 {
        let direction = env
            .call_method(
                &gainmap,
                jni_str!("getGainmapDirection"),
                jni_sig!("()I"),
                &[],
            )?
            .i()?;
        ensure!(
            direction == 0 || direction == 1,
            "Unknown gainmap direction {direction}"
        );
        hdr_base = direction == 1;
        let color_space = env
            .call_method(
                &gainmap,
                jni_str!("getAlternativeImagePrimaries"),
                jni_sig!("()Landroid/graphics/ColorSpace;"),
                &[],
            )?
            .l()?;
        if !color_space.is_null() {
            icc = Some(alternate_icc(env, &color_space)?);
        }
    }
    let metadata = GainMapParameters {
        ratio_min: float_array(env, &gainmap, "getRatioMin")?,
        ratio_max: float_array(env, &gainmap, "getRatioMax")?,
        gamma: float_array(env, &gainmap, "getGamma")?,
        epsilon_sdr: float_array(env, &gainmap, "getEpsilonSdr")?,
        epsilon_hdr: float_array(env, &gainmap, "getEpsilonHdr")?,
        display_ratio_sdr: float(env, &gainmap, "getMinDisplayRatioForHdrTransition")?,
        display_ratio_hdr: float(env, &gainmap, "getDisplayRatioForFullHdr")?,
        hdr_base,
        use_base_color_space: icc.is_none(),
    }
    .to_iso()?;
    let contents = env
        .call_method(
            &gainmap,
            jni_str!("getGainmapContents"),
            jni_sig!("()Landroid/graphics/Bitmap;"),
            &[],
        )?
        .l()?;
    ensure!(!contents.is_null(), "Gainmap contents are null");
    // SAFETY: getGainmapContents returns a Bitmap, held alive by contents.
    let bitmap = unsafe { get_bitmap_data(env, contents.as_raw())? };
    let (width, height) = (bitmap.width, bitmap.height);
    let rgb8 = |rgba: &[u8]| {
        rgba.as_chunks::<4>()
            .0
            .iter()
            .flat_map(|p| [p[0], p[1], p[2]])
            .collect()
    };
    let rgb16 = |rgba: &[u16]| {
        rgba.as_chunks::<4>()
            .0
            .iter()
            .flat_map(|p| [p[0], p[1], p[2]])
            .collect()
    };
    // Gainmap samples are numerical data: ignore their Bitmap color space and
    // alpha, and retain the map's own resolution independently of the base.
    let mut map = match bitmap.format {
        BitmapPixelFormat::A8 => GainMap::gray8(bitmap.data, width, height, metadata),
        BitmapPixelFormat::Rgba8888 => GainMap::rgb8(rgb8(&bitmap.data), width, height, metadata),
        BitmapPixelFormat::Rgb565 => GainMap::rgb8(
            rgb8(&rgb565_bytes_to_rgba8888(&bitmap.data)),
            width,
            height,
            metadata,
        ),
        BitmapPixelFormat::Rgba1010102 => GainMap::rgb16(
            rgb16(&ar30_bytes_to_rgba10(&bitmap.data)),
            10,
            width,
            height,
            metadata,
        ),
        BitmapPixelFormat::RgbaF16 => GainMap::rgb16(
            rgb16(&f16_bytes_to_rgba10(&bitmap.data, width, height)?),
            10,
            width,
            height,
            metadata,
        ),
    }
    .with_lossless(lossless);
    if let Some(icc) = icc {
        map = map.with_alternate_icc_profile(icc);
    }
    Ok(Some(map))
}
