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
use crate::cvt::{
    pack_10_or_12_to_565, pack_10_or_12_to_ar30, pack_10_or_12_to_f16, pack_8_to_565,
    pack_8_to_ar30, pack_8_to_f16,
};
use crate::ffi::{
    create_rgba8888_hardware_buffer, create_rgba8888_hardware_buffer_from_u16, software_bitmap,
    wrap_hardware_buffer,
};
use crate::jxl_decode::{
    decode_packed_jxl, is_jxl, read_jxl_info, BitDepth, DecodedJxlPacket, PackedJxl, WeaverError,
};
use crate::scaling::{internal_scale_u16, internal_scale_u8};
use crate::support::{
    android_os_version, dbg_log, init_logging, try_vec, PackedImageBuffer, PackedImageTransfer,
    MIN_OS_AR30, MIN_OS_F16,
};
use crate::{WeaveScaleMode, WeaverPreferredColorConfig};
use jni::objects::JObject;
use jni::strings::JNIString;
use jni::sys::jobject;
use jni::{jni_str, Env, EnvUnowned, Outcome};
use moxcms::{ColorProfile, Layout, TransformOptions};
use std::ptr::null_mut;
use std::slice;

fn make_8bit_transfer(
    env: &mut Env,
    data: &[u8],
    width: usize,
    height: usize,
    preferred_color: WeaverPreferredColorConfig,
) -> Result<PackedImageTransfer, anyhow::Error> {
    match preferred_color {
        WeaverPreferredColorConfig::Default | WeaverPreferredColorConfig::Rgba8888 => {
            Ok(PackedImageTransfer::Image(PackedImageBuffer {
                data: data.to_vec(),
                width,
                height,
                format: WeaverPreferredColorConfig::Rgba8888,
            }))
        }
        WeaverPreferredColorConfig::RgbaF16 => {
            if android_os_version() >= MIN_OS_F16 {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: pack_8_to_f16(data, width, height)?,
                    width,
                    height,
                    format: WeaverPreferredColorConfig::RgbaF16,
                }))
            } else {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: data.to_vec(),
                    width,
                    height,
                    format: WeaverPreferredColorConfig::RgbaF16,
                }))
            }
        }
        WeaverPreferredColorConfig::Rgb565 => Ok(PackedImageTransfer::Image(PackedImageBuffer {
            data: pack_8_to_565(data, width, height)?,
            width,
            height,
            format: WeaverPreferredColorConfig::Rgb565,
        })),
        WeaverPreferredColorConfig::Rgba1010102 => {
            if android_os_version() >= MIN_OS_AR30 {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: pack_8_to_ar30(data, width, height)?,
                    width,
                    height,
                    format: WeaverPreferredColorConfig::Rgba1010102,
                }))
            } else {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: data.to_vec(),
                    width,
                    height,
                    format: WeaverPreferredColorConfig::RgbaF16,
                }))
            }
        }
        WeaverPreferredColorConfig::Hardware => {
            match create_rgba8888_hardware_buffer(env, data, width, height) {
                Ok(Some(hardware_buffer)) => {
                    Ok(PackedImageTransfer::HardwareBuffer(hardware_buffer))
                }
                Ok(None) => Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: data.to_vec(),
                    width,
                    height,
                    format: WeaverPreferredColorConfig::Rgba8888,
                })),
                Err(_error) => {
                    dbg_log!(
                        warn,
                        "Hardware RGBA8888 transfer failed for {}x{}; falling back to software bitmap: {_error:#}",
                        width,
                        height
                    );
                    Ok(PackedImageTransfer::Image(PackedImageBuffer {
                        data: data.to_vec(),
                        width,
                        height,
                        format: WeaverPreferredColorConfig::Rgba8888,
                    }))
                }
            }
        }
    }
}

fn make_10_12bit_transfer(
    env: &mut Env,
    data: &[u16],
    width: usize,
    height: usize,
    bit_depth: BitDepth,
    preferred_color: WeaverPreferredColorConfig,
) -> Result<PackedImageTransfer, anyhow::Error> {
    let format_8_bit = || {
        let diff = bit_depth.bits().saturating_sub(8) as u32;
        Ok(PackedImageTransfer::Image(PackedImageBuffer {
            data: data
                .iter()
                .map(|x| (x >> diff).min(255) as u8)
                .collect::<Vec<u8>>(),
            width,
            height,
            format: WeaverPreferredColorConfig::Rgba8888,
        }))
    };
    match preferred_color {
        WeaverPreferredColorConfig::Default | WeaverPreferredColorConfig::Rgba8888 => {
            format_8_bit()
        }
        WeaverPreferredColorConfig::RgbaF16 => {
            if android_os_version() >= MIN_OS_F16 {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: pack_10_or_12_to_f16(data, width, height, bit_depth.bits() as usize)?,
                    width,
                    height,
                    format: WeaverPreferredColorConfig::RgbaF16,
                }))
            } else {
                format_8_bit()
            }
        }
        WeaverPreferredColorConfig::Rgb565 => Ok(PackedImageTransfer::Image(PackedImageBuffer {
            data: pack_10_or_12_to_565(data, width, height, bit_depth.bits() as usize)?,
            width,
            height,
            format: WeaverPreferredColorConfig::Rgb565,
        })),
        WeaverPreferredColorConfig::Rgba1010102 => {
            if android_os_version() >= MIN_OS_AR30 {
                Ok(PackedImageTransfer::Image(PackedImageBuffer {
                    data: pack_10_or_12_to_ar30(data, width, height, bit_depth.bits() as usize)?,
                    width,
                    height,
                    format: WeaverPreferredColorConfig::Rgba1010102,
                }))
            } else {
                format_8_bit()
            }
        }
        WeaverPreferredColorConfig::Hardware => {
            match create_rgba8888_hardware_buffer_from_u16(
                env,
                data,
                width,
                height,
                bit_depth.bits() as usize,
            ) {
                Ok(Some(hardware_buffer)) => {
                    Ok(PackedImageTransfer::HardwareBuffer(hardware_buffer))
                }
                Ok(None) => format_8_bit(),
                Err(_error) => {
                    dbg_log!(
                        warn,
                        "Hardware high-bit-depth transfer failed for {}x{} at {} bits; falling back to software bitmap: {_error:#}",
                        width,
                        height,
                        bit_depth.bits()
                    );
                    format_8_bit()
                }
            }
        }
    }
}

pub(crate) fn packed_jxl_to_bitmap<'local>(
    env: &mut Env<'local>,
    decode: PackedJxl,
    scaled_width: i32,
    scaled_height: i32,
    scale_mode: WeaveScaleMode,
    preferred_color: WeaverPreferredColorConfig,
) -> Result<JObject<'local>, anyhow::Error> {
    match decode {
        PackedJxl::Regular(regular_image) => {
            let DecodedJxlPacket {
                data: source_data,
                width,
                height,
                icc,
                bit_depth: _,
                has_real_alpha,
            } = regular_image;
            let _source_len = source_data.len();
            dbg_log!(
                debug,
                "Regular 8-bit JPEG XL image: {}x{}, has_alpha={}, source_bytes={}, requested={}x{}, mode={:?}",
                width,
                height,
                has_real_alpha,
                _source_len,
                scaled_width,
                scaled_height,
                scale_mode
            );

            // Transfer ownership to the scaler. Its source store is dropped
            // before this call returns, so the full-resolution RGBA allocation
            // is no longer alive while the hardware buffer is allocated/locked.
            let mut scaled_data = internal_scale_u8(
                std::borrow::Cow::Owned(source_data),
                width as u32 * 4,
                width as u32,
                height as u32,
                scaled_width,
                scaled_height,
                has_real_alpha,
                scale_mode,
            )?;

            dbg_log!(
                debug,
                "8-bit scaling complete: source_bytes_released={}, output={}x{}, output_bytes={}",
                _source_len,
                scaled_data.width,
                scaled_data.height,
                scaled_data.buffer.borrow().len()
            );

            if let Some(transform) = icc.as_ref().and_then(|icc| {
                ColorProfile::new_from_slice(icc)
                    .and_then(|x| {
                        x.create_transform_8bit(
                            Layout::Rgba,
                            &ColorProfile::new_srgb(),
                            Layout::Rgba,
                            TransformOptions::default(),
                        )
                    })
                    .ok()
            }) {
                dbg_log!(debug, "8-bit: applying ICC profile transform");
                let mut target_data = try_vec![0u8; scaled_data.buffer.borrow().len()];
                transform
                    .transform(scaled_data.buffer.borrow(), &mut target_data)
                    .map_err(|x| {
                        dbg_log!(error, "ICC transform failed: {x}");
                        anyhow::anyhow!(x)
                    })?;
                scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
            } else {
                dbg_log!(debug, "8-bit: no usable embedded ICC profile");
            }

            let new_width = scaled_data.width;
            let new_height = scaled_data.height;

            let transfer = make_8bit_transfer(
                env,
                scaled_data.buffer.borrow(),
                new_width,
                new_height,
                preferred_color,
            )?;

            let result = Ok(match transfer {
                PackedImageTransfer::Image(image) => {
                    dbg_log!(
                        debug,
                        "8-bit: creating software bitmap {}x{} fmt={:?}",
                        image.width,
                        image.height,
                        image.format
                    );
                    software_bitmap(env, &image, None).map_err(|x| {
                        dbg_log!(error, "software_bitmap failed: {x}");
                        anyhow::anyhow!(x)
                    })?
                }
                PackedImageTransfer::HardwareBuffer(hb) => {
                    dbg_log!(debug, "8-bit: wrapping hardware buffer");
                    wrap_hardware_buffer(env, hb, None).map_err(|x| {
                        dbg_log!(error, "wrap_hardware_buffer failed: {x}");
                        anyhow::anyhow!(x)
                    })?
                }
            });

            dbg_log!(debug, "8-bit: decode_pipeline complete");
            result
        }
        PackedJxl::HighBitDepth(hd_image) => {
            let DecodedJxlPacket {
                data: source_data,
                width,
                height,
                icc,
                bit_depth,
                has_real_alpha,
            } = hd_image;
            let _source_samples = source_data.len();
            dbg_log!(
                debug,
                "High-bit-depth JPEG XL image: {}x{}, depth={}, has_alpha={}, source_samples={}, requested={}x{}, mode={:?}",
                width,
                height,
                bit_depth.bits(),
                has_real_alpha,
                _source_samples,
                scaled_width,
                scaled_height,
                scale_mode
            );

            // As in the 8-bit path, ownership lets the scaler release the large
            // decoded source before AHardwareBuffer allocation and mapping.
            let mut scaled_data = internal_scale_u16(
                std::borrow::Cow::Owned(source_data),
                width * 4,
                width as u32,
                height as u32,
                scaled_width,
                scaled_height,
                bit_depth.bits() as usize,
                has_real_alpha,
                scale_mode,
            )?;
            dbg_log!(
                debug,
                "High-bit-depth scaling complete: source_samples_released={}, output={}x{}, output_samples={}",
                _source_samples,
                scaled_data.width,
                scaled_data.height,
                scaled_data.buffer.borrow().len()
            );
            if let Some(transform) = icc.as_ref().and_then(|icc| {
                ColorProfile::new_from_slice(icc)
                    .and_then(|x| {
                        x.create_transform_16bit(
                            Layout::Rgba,
                            &ColorProfile::new_srgb(),
                            Layout::Rgba,
                            TransformOptions::default(),
                        )
                    })
                    .ok()
            }) {
                let mut target_data = try_vec![0u16; scaled_data.buffer.borrow().len()];
                transform
                    .transform(scaled_data.buffer.borrow(), &mut target_data)
                    .map_err(|x| anyhow::anyhow!(x))?;
                scaled_data.buffer = pic_scale::BufferStore::Owned(target_data);
            }

            let new_width = scaled_data.width;
            let new_height = scaled_data.height;
            let transfer = make_10_12bit_transfer(
                env,
                scaled_data.buffer.borrow(),
                new_width,
                new_height,
                bit_depth,
                preferred_color,
            )?;
            Ok(match transfer {
                PackedImageTransfer::Image(image) => {
                    software_bitmap(env, &image, None).map_err(|x| anyhow::anyhow!(x))?
                }
                PackedImageTransfer::HardwareBuffer(hb) => {
                    wrap_hardware_buffer(env, hb, None).map_err(|x| anyhow::anyhow!(x))?
                }
            })
        }
    }
}

fn decode_pipeline<'local>(
    env: &mut Env<'local>,
    data: &[u8],
    scaled_width: i32,
    scaled_height: i32,
    scale_mode: WeaveScaleMode,
    preferred_color: WeaverPreferredColorConfig,
) -> Result<JObject<'local>, anyhow::Error> {
    let decode = decode_packed_jxl(data).map_err(|x| anyhow::anyhow!(x))?;
    packed_jxl_to_bitmap(
        env,
        decode,
        scaled_width,
        scaled_height,
        scale_mode,
        preferred_color,
    )
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn decode_jxl_file(
    env: *mut jni::sys::JNIEnv,
    data: *const u8,
    length: usize,
    scaled_width: i32,
    scaled_height: i32,
    scale_mode: WeaveScaleMode,
    preferred_color_config: WeaverPreferredColorConfig,
) -> jobject {
    init_logging();

    dbg_log!(
        debug,
        "decode_jxl_file called: length={length}, \
        target={}x{}, scale={:?}, color={:?}",
        scaled_width,
        scaled_height,
        scale_mode,
        preferred_color_config
    );

    let mut unowned = unsafe { EnvUnowned::from_raw(env) };

    let outcome = unowned.with_env(|env| -> Result<jobject, jni::errors::Error> {
        let bytes = if length == 0 {
            &[]
        } else if data.is_null() {
            let _ = env.throw_new(
                jni_str!("java/lang/IllegalArgumentException"),
                JNIString::from("JPEG XL input pointer is null"),
            );
            return Ok(JObject::null().as_raw());
        } else {
            unsafe { slice::from_raw_parts(data, length) }
        };

        let mut decoding_result = || -> Result<jobject, jni::errors::Error> {
            match decode_pipeline(
                env,
                bytes,
                scaled_width,
                scaled_height,
                scale_mode,
                preferred_color_config,
            ) {
                Ok(obj) => {
                    dbg_log!(debug, "decode_jxl_file succeeded");
                    Ok(obj.as_raw())
                }
                Err(e) => {
                    dbg_log!(error, "decode_jxl_file failed: {e:#}");
                    let _ = env.throw_new(
                        jni_str!("java/lang/RuntimeException"),
                        JNIString::from(e.to_string()),
                    );
                    Ok(JObject::null().as_raw())
                }
            }
        };
        let result = decoding_result();
        match result {
            Ok(obj) => {
                dbg_log!(debug, "decode_jxl_file: success");
                Ok(obj)
            }
            Err(e) => {
                dbg_log!(error, "decode_jxl_file failed: {e:#}");
                let _ = env.throw_new(
                    jni_str!("java/lang/RuntimeException"),
                    JNIString::from(e.to_string()),
                );
                Ok(JObject::null().into_raw())
            }
        }
    });

    let o = outcome.into_outcome();
    match o {
        Outcome::Ok(v) => v,
        Outcome::Err(_e) => {
            dbg_log!(error, "JNI error in with_env: {_e:?}");
            null_mut()
        }
        Outcome::Panic(_p) => {
            let _msg = _p
                .downcast_ref::<&str>()
                .map(|s| s.to_string())
                .or_else(|| _p.downcast_ref::<String>().cloned())
                .unwrap_or_else(|| "unknown panic".to_string());
            dbg_log!(error, "panic in with_env: {_msg:?}");
            null_mut()
        }
    }
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct JxlInfo {
    pub supported_image: bool,
    pub width: u32,
    pub height: u32,
    pub bit_depth: u32,
}

impl JxlInfo {
    pub(crate) fn not_a_jxl() -> JxlInfo {
        JxlInfo {
            supported_image: false,
            width: 0,
            height: 0,
            bit_depth: 0,
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn read_jxl_file_info(data: *const u8, length: usize) -> JxlInfo {
    if data.is_null() || length == 0 {
        return JxlInfo::not_a_jxl();
    }
    let bytes = unsafe { slice::from_raw_parts(data, length) };
    if !is_jxl(bytes) {
        return JxlInfo::not_a_jxl();
    }
    match read_jxl_info(bytes) {
        Ok((width, height, bit_depth)) => {
            let Ok(width) = u32::try_from(width) else {
                return JxlInfo::not_a_jxl();
            };
            let Ok(height) = u32::try_from(height) else {
                return JxlInfo::not_a_jxl();
            };
            JxlInfo {
                width,
                height,
                supported_image: true,
                bit_depth,
            }
        }
        Err(_v) => {
            dbg_log!(error, "Failed to read JPEG XL info: {_v:?}");
            JxlInfo::not_a_jxl()
        }
    }
}
