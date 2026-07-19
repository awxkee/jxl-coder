/*
 * Copyright (c) Radzivon Bartoshyk 2026/6. All rights reserved.
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
use crate::cvt::{ar30_bytes_to_rgba10, f16_bytes_to_rgba10, rgb565_bytes_to_rgba8888};
use crate::ffi::{get_bitmap_data, BitmapData, BitmapPixelFormat};
use crate::jxl_decode::WeaverError;
use crate::support::{
    dbg_log, has_non_constant_alpha, init_logging, optional_bytebuffer_to_vec,
    panic_payload_to_string, throw_runtime_exception, throw_runtime_exception_raw,
};
use crate::JixelEncodingSpeed;
use jixel::{Primaries, RenderingIntent, TransferFunction, WhitePoint};
use jni::objects::JObject;
use jni::sys::{jbyteArray, jobject};
use jni::{EnvUnowned, Outcome};
use ndk_sys::ADataSpace;
use std::num::NonZero;
use std::ptr::null_mut;
use std::thread::available_parallelism;

impl JixelEncodingSpeed {
    pub(crate) fn to_jixel(&self) -> jixel::Speed {
        match self {
            JixelEncodingSpeed::Slow => jixel::Speed::Slow,
            JixelEncodingSpeed::Fast => jixel::Speed::Fast,
        }
    }
}

pub(crate) fn resolve_cicp_jixel(color_space: i32) -> jixel::ColorEncoding {
    let ds = ADataSpace(color_space);

    if ds == ADataSpace::ADATASPACE_UNKNOWN {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt601,
            transfer: TransferFunction::Srgb,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    if ds == ADataSpace::ADATASPACE_SCRGB {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt601,
            transfer: TransferFunction::Srgb,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    if ds == ADataSpace::ADATASPACE_BT601_525 || ds == ADataSpace::ADATASPACE_BT601_625 {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt601,
            transfer: TransferFunction::Bt601,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    // BT.709 video (limited range)
    if ds == ADataSpace::ADATASPACE_BT709 {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt709,
            transfer: TransferFunction::Bt709,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    // sRGB — BT.709 primaries + matrix, sRGB transfer, full range
    if ds == ADataSpace::ADATASPACE_SRGB {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt709,
            transfer: TransferFunction::Srgb,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    // Linear sRGB (scRGB linear) — BT.709 primaries + matrix, linear transfer
    if ds == ADataSpace::ADATASPACE_SCRGB_LINEAR {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt709,
            transfer: TransferFunction::Linear,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    if ds == ADataSpace::ADATASPACE_DISPLAY_P3 {
        return jixel::ColorEncoding {
            primaries: Primaries::Smpte432,
            transfer: TransferFunction::Srgb,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    if ds == ADataSpace::ADATASPACE_DCI_P3 {
        return jixel::ColorEncoding {
            primaries: Primaries::Smpte431,
            transfer: TransferFunction::Smpte428,
            white_point: WhitePoint::Dci,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    // BT.2020 SDR
    if ds == ADataSpace::ADATASPACE_BT2020 {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt2020,
            transfer: TransferFunction::Bt202010bit,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    // BT.2020 PQ — full-range variant
    if ds == ADataSpace::ADATASPACE_BT2020_PQ {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt2020,
            transfer: TransferFunction::Smpte2084,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    // BT.2020 PQ — ITU limited-range variant
    if ds == ADataSpace::ADATASPACE_BT2020_ITU_PQ {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt2020,
            transfer: TransferFunction::Smpte2084,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    // BT.2020 HLG — full-range variant
    if ds == ADataSpace::ADATASPACE_BT2020_HLG {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt2020,
            transfer: TransferFunction::Hlg,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    // BT.2020 HLG — ITU limited-range variant
    if ds == ADataSpace::ADATASPACE_BT2020_ITU_HLG {
        return jixel::ColorEncoding {
            primaries: Primaries::Bt2020,
            transfer: TransferFunction::Hlg,
            white_point: WhitePoint::D65,
            rendering_intent: RenderingIntent::Perceptual,
        };
    }

    jixel::ColorEncoding::srgb()
}

fn encode_jixel_inner_u8(
    bitmap_data: &BitmapData,
    config: &JixelEncodingConfig,
    has_real_alpha: bool,
) -> Result<Vec<u8>, anyhow::Error> {
    let quality = config.quality;
    let lossless = config.lossless;
    let exif = config.exif.as_ref();
    dbg_log!(
        debug,
        "encode_av1_inner_u8: {}x{} pixels={} quality={} lossless={} \
         exif={}",
        bitmap_data.width,
        bitmap_data.height,
        bitmap_data.data.len(),
        quality,
        lossless,
        exif.map_or_else(|| "none".to_string(), |e| format!("{} bytes", e.len()))
    );

    let local_cicp = config.color_encoding;
    dbg_log!(debug, "has_real_alpha={has_real_alpha}");

    let threads = available_parallelism()
        .unwrap_or(NonZero::new(1).unwrap())
        .get();
    dbg_log!(debug, "encoder threads={threads}");

    let mut config = jixel::EncodeConfig::default()
        .with_color_encoding(local_cicp)
        .with_quality(quality as f32)
        .with_num_threads(threads)
        .with_speed(config.speed.to_jixel())
        .with_progressive(false)
        .with_lossless(lossless);

    if let Some(exif) = exif {
        dbg_log!(debug, "attaching exif: {} bytes", exif.len());
        config = config.with_exif(exif.to_vec());
    }

    if has_real_alpha {
        let alpha = bitmap_data
            .data
            .as_chunks::<4>()
            .0
            .iter()
            .map(|x| x[3])
            .collect::<Vec<_>>();
        dbg_log!(debug, "alpha plane: {} samples", alpha.len());

        let result = jixel::encode_image_with_alpha(
            &bitmap_data.data,
            bitmap_data.width,
            bitmap_data.height,
            &config,
        )
        .map_err(|x| {
            dbg_log!(error, "encode_yuva8_with_alpha failed: {x}");
            anyhow::anyhow!(x)
        })?;
        return Ok(result);
    }

    let discarded_alpha = bitmap_data
        .data
        .as_chunks::<4>()
        .0
        .iter()
        .flat_map(|x| [x[0], x[1], x[2]])
        .collect::<Vec<_>>();

    let result = jixel::encode_image(
        &discarded_alpha,
        bitmap_data.width,
        bitmap_data.height,
        &config,
    )
    .map_err(|x| {
        dbg_log!(error, "encode_yuv8 failed: {x}");
        anyhow::anyhow!(x)
    })?;
    dbg_log!(
        debug,
        "encoded {:?}: bytes ({:.2} bpp)",
        result.len(),
        (result.len() * 8) as f64 / (bitmap_data.width * bitmap_data.height) as f64
    );
    Ok(result)
}

fn encode_av1_inner_u16_10_bit(
    hd_plane: &[u16],
    bitmap_data: &BitmapData,
    config: &JixelEncodingConfig,
    has_real_alpha: bool,
) -> Result<Vec<u8>, anyhow::Error> {
    let quality = config.quality;
    let lossless = config.lossless;
    let exif = config.exif.as_ref();
    dbg_log!(
        debug,
        "encode_av1_inner_u8: {}x{} pixels={} quality={} lossless={} \
         exif={}",
        bitmap_data.width,
        bitmap_data.height,
        bitmap_data.data.len(),
        quality,
        lossless,
        exif.map_or_else(|| "none".to_string(), |e| format!("{} bytes", e.len()))
    );

    let mut local_cicp = config.color_encoding;
    dbg_log!(debug, "has_real_alpha={has_real_alpha}");

    let threads = available_parallelism()
        .unwrap_or(NonZero::new(1).unwrap())
        .get();
    dbg_log!(debug, "encoder threads={threads}");

    let mut config = jixel::EncodeConfig::default()
        .with_color_encoding(local_cicp)
        .with_quality(quality as f32)
        .with_num_threads(threads)
        .with_speed(config.speed.to_jixel())
        .with_progressive(false)
        .with_lossless(lossless);

    if let Some(exif) = exif {
        dbg_log!(debug, "attaching exif: {} bytes", exif.len());
        config = config.with_exif(exif.to_vec());
    }

    if has_real_alpha {
        let alpha = bitmap_data
            .data
            .as_chunks::<4>()
            .0
            .iter()
            .map(|x| x[3])
            .collect::<Vec<_>>();
        dbg_log!(debug, "alpha plane: {} samples", alpha.len());

        let result = jixel::encode_image_with_alpha_10bit(
            &hd_plane,
            bitmap_data.width,
            bitmap_data.height,
            &config,
        )
        .map_err(|x| {
            dbg_log!(error, "encode_yuva8_with_alpha failed: {x}");
            anyhow::anyhow!(x)
        })?;
        return Ok(result);
    }

    let discarded_alpha = hd_plane
        .as_chunks::<4>()
        .0
        .iter()
        .flat_map(|x| [x[0], x[1], x[2]])
        .collect::<Vec<_>>();

    let result = jixel::encode_image_10bit(
        &discarded_alpha,
        bitmap_data.width,
        bitmap_data.height,
        &config,
    )
    .map_err(|x| {
        dbg_log!(error, "encode_yuv8 failed: {x}");
        anyhow::anyhow!(x)
    })?;
    dbg_log!(
        debug,
        "encoded {:?}: bytes ({:.2} bpp)",
        result.len(),
        (result.len() * 8) as f64 / (bitmap_data.width * bitmap_data.height) as f64
    );
    Ok(result)
}

fn encode_jixel_inner(
    bitmap_data: &mut BitmapData,
    config: &JixelEncodingConfig,
) -> Result<Vec<u8>, anyhow::Error> {
    dbg_log!(debug, "encode_av1_inner: format={:?}", bitmap_data.format,);
    match bitmap_data.format {
        BitmapPixelFormat::Rgba8888 => {
            let has_real_alpha =
                has_non_constant_alpha::<u8, u32, 3, 4>(&bitmap_data.data, bitmap_data.width);
            encode_jixel_inner_u8(bitmap_data, config, has_real_alpha)
        }
        BitmapPixelFormat::Rgb565 => {
            dbg_log!(
                debug,
                "encode_av1_inner: converting Rgb565 → Rgba8888 before encode"
            );
            let rgba8888 = rgb565_bytes_to_rgba8888(&bitmap_data.data);
            bitmap_data.data = rgba8888;
            encode_jixel_inner_u8(bitmap_data, config, false)
        }
        BitmapPixelFormat::RgbaF16 => {
            dbg_log!(
                debug,
                "encode_av1_inner: converting RgbaF16 → RGBA10 before encode"
            );
            let hd_plane =
                f16_bytes_to_rgba10(&bitmap_data.data, bitmap_data.width, bitmap_data.height)?;
            encode_av1_inner_u16_10_bit(&hd_plane, bitmap_data, config, false)
        }
        BitmapPixelFormat::Rgba1010102 => {
            dbg_log!(
                debug,
                "encode_av1_inner: unpacking AR30 → RGBA10 before encode"
            );
            let hd_plane = ar30_bytes_to_rgba10(&bitmap_data.data);
            encode_av1_inner_u16_10_bit(&hd_plane, bitmap_data, config, false)
        }
        BitmapPixelFormat::A8 => {
            dbg_log!(error, "encode_av1_inner: A8 format is not supported");
            Err(anyhow::anyhow!(WeaverError::PixelFormatIsNotSupported(
                "BitmapPixelFormat::A8".to_string()
            )))
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct JixelEncodingConfig {
    pub(crate) quality: u32,
    pub(crate) color_encoding: jixel::ColorEncoding,
    pub(crate) exif: Option<Vec<u8>>,
    pub(crate) lossless: bool,
    pub(crate) speed: JixelEncodingSpeed,
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn encode_jixel_file(
    env: *mut jni::sys::JNIEnv,
    image: jobject,
    exif: jobject,
    color_space: i32,
    quality: i32,
    lossless: bool,
    speed: JixelEncodingSpeed,
) -> jbyteArray {
    init_logging();

    dbg_log!(
        debug,
        "encode_avif_file: color_space={color_space} quality={quality} lossless={lossless} \
         image_null={} exif_null={}",
        image.is_null(),
        exif.is_null(),
    );

    let mut unowned = unsafe { EnvUnowned::from_raw(env) };

    let outcome = unowned.with_env(|env| -> Result<jobject, jni::errors::Error> {
        let result: Result<jobject, anyhow::Error> = (|| {
            let quality = quality.clamp(1, 100) as u32;
            dbg_log!(debug, "clamped quality={quality}");

            let mut bitmap_data = unsafe {
                get_bitmap_data(env, image).map_err(|x| {
                    dbg_log!(error, "get_bitmap_data failed: {x}");
                    anyhow::anyhow!(x)
                })?
            };
            dbg_log!(
                debug,
                "bitmap: {}x{} format={:?} data={} bytes",
                bitmap_data.width,
                bitmap_data.height,
                bitmap_data.format,
                bitmap_data.data.len()
            );

            let exif_data = optional_bytebuffer_to_vec(env, exif).map_err(|x| {
                dbg_log!(error, "optional_bytebuffer_to_vec failed: {x}");
                anyhow::anyhow!(x)
            })?;
            dbg_log!(
                debug,
                "exif: {}",
                exif_data
                    .as_ref()
                    .map_or_else(|| "none".to_string(), |e| format!("{} bytes", e.len()))
            );

            let encoded_data = encode_jixel_inner(
                &mut bitmap_data,
                &JixelEncodingConfig {
                    color_encoding: resolve_cicp_jixel(color_space),
                    quality,
                    lossless,
                    exif: exif_data.map(|x| x.to_vec()),
                    speed,
                },
            )
            .map_err(|x| {
                dbg_log!(error, "encode_av1_inner failed: {x:#}");
                x
            })?;
            dbg_log!(
                debug,
                "encode complete: {} bytes output",
                encoded_data.len()
            );

            let arr = env.byte_array_from_slice(&encoded_data).map_err(|e| {
                dbg_log!(error, "byte_array_from_slice failed: {e}");
                anyhow::anyhow!(e)
            })?;
            dbg_log!(debug, "java byte[] created successfully");
            Ok(arr.into_raw())
        })();

        match result {
            Ok(obj) => {
                dbg_log!(debug, "encode_avif_file: success");
                Ok(obj)
            }
            Err(e) => {
                dbg_log!(error, "encode_avif_file failed: {e:#}");
                throw_runtime_exception(env, format!("AVIF/AV1 encoding failed: {e:#}"));
                Ok(JObject::null().into_raw())
            }
        }
    });

    let o = outcome.into_outcome();
    match o {
        Outcome::Ok(v) => {
            dbg_log!(debug, "encode_avif_file: success");
            v
        }
        Outcome::Err(_e) => {
            let msg = format!("JNI error while encoding AVIF/AV1: {_e}");
            dbg_log!(error, "{msg}");
            unsafe { throw_runtime_exception_raw(env, msg) };
            null_mut()
        }
        Outcome::Panic(_p) => {
            let msg = format!(
                "panic while encoding AVIF/AV1: {}",
                panic_payload_to_string(_p.as_ref())
            );
            dbg_log!(error, "{msg}");
            unsafe { throw_runtime_exception_raw(env, msg) };
            null_mut()
        }
    }
}
