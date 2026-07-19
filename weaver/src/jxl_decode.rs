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

use crate::check_image_size_overflow;
use crate::support::try_vec;
use jxl::api::{
    check_signature, Endianness, JxlColorType, JxlDataFormat, JxlDecoder, JxlDecoderOptions,
    JxlOutputBuffer, JxlPixelFormat, ProcessingResult,
};
use jxl::headers::extra_channels::ExtraChannel;
use std::mem::size_of;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum WeaverError {
    #[error("Data is not a JPEG XL image")]
    InvalidJxl,
    #[error("JPEG XL decoder failed: {0}")]
    FailedToDecodeJxl(String),
    #[error("Failed to allocate memory with size {0}")]
    FailedToAllocateMemory(u64),
    #[error("Pixel format is not supported: {0}")]
    PixelFormatIsNotSupported(String),
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub(crate) struct BitDepth(u8);

impl BitDepth {
    pub(crate) fn new(bits: u32) -> Result<Self, WeaverError> {
        let bits = u8::try_from(bits).map_err(|_| {
            WeaverError::PixelFormatIsNotSupported(format!("{bits}-bit JPEG XL samples"))
        })?;
        if !(1..=16).contains(&bits) {
            return Err(WeaverError::PixelFormatIsNotSupported(format!(
                "{bits}-bit JPEG XL samples"
            )));
        }
        Ok(Self(bits))
    }

    pub(crate) fn bits(self) -> u8 {
        self.0
    }
}

pub(crate) struct DecodedJxlPacket<T> {
    pub(crate) data: Vec<T>,
    pub(crate) width: usize,
    pub(crate) height: usize,
    pub(crate) icc: Option<Vec<u8>>,
    pub(crate) bit_depth: BitDepth,
    pub(crate) has_real_alpha: bool,
}

pub(crate) enum PackedJxl {
    Regular(DecodedJxlPacket<u8>),
    HighBitDepth(DecodedJxlPacket<u16>),
}

pub(crate) fn is_jxl(bytes: &[u8]) -> bool {
    matches!(
        check_signature(bytes),
        ProcessingResult::Complete { result: Some(_) }
    )
}

fn decode_error(error: impl std::fmt::Display) -> WeaverError {
    WeaverError::FailedToDecodeJxl(error.to_string())
}

pub(crate) fn decode_packed_jxl(data: &[u8]) -> Result<PackedJxl, WeaverError> {
    if !is_jxl(data) {
        return Err(WeaverError::InvalidJxl);
    }

    let mut input = data;
    let mut decoder = match JxlDecoder::new(JxlDecoderOptions::default())
        .process(&mut input)
        .map_err(decode_error)?
    {
        ProcessingResult::Complete { result } => result,
        ProcessingResult::NeedsMoreInput { .. } => {
            return Err(WeaverError::FailedToDecodeJxl(
                "truncated before image metadata".into(),
            ));
        }
    };

    let info = decoder.basic_info();
    let (width, height) = info.size;
    let source_bit_depth = info.bit_depth.bits_per_sample();
    // Android's bitmap paths consume full-range 8- or 16-bit RGBA. Asking
    // jxl-rs to normalize here also handles uncommon JXL depths (for example
    // 5, 9, or 14 bits) without teaching every downstream converter about them.
    let bit_depth = BitDepth::new(if source_bit_depth <= 8 { 8 } else { 16 })?;
    if check_image_size_overflow(width as u64, height as u64, 4, 2) {
        return Err(WeaverError::FailedToAllocateMemory(isize::MAX as u64));
    }

    let has_real_alpha = info
        .extra_channels
        .iter()
        .any(|channel| channel.ec_type == ExtraChannel::Alpha);
    let extra_channels = info.extra_channels.len();

    let format = if bit_depth.bits() <= 8 {
        JxlDataFormat::U8 { bit_depth: 8 }
    } else {
        JxlDataFormat::U16 {
            endianness: Endianness::native(),
            bit_depth: 16,
        }
    };
    decoder.set_pixel_format(JxlPixelFormat {
        color_type: JxlColorType::Rgba,
        color_data_format: Some(format),
        // Alpha is requested interleaved through Rgba; all other extra channels
        // are deliberately ignored by this still-image Android API.
        extra_channel_format: vec![None; extra_channels],
    });
    // This must describe the pixels jxl-rs actually emits. For XYB files with
    // an ICC profile and no external CMS, jxl-rs intentionally falls back to
    // sRGB output, so using the embedded profile here would double-transform.
    let icc = decoder
        .output_color_profile()
        .try_as_icc()
        .map(|profile| profile.into_owned());

    let decoder = match decoder.process(&mut input).map_err(decode_error)? {
        ProcessingResult::Complete { result } => result,
        ProcessingResult::NeedsMoreInput { .. } => {
            return Err(WeaverError::FailedToDecodeJxl(
                "truncated before frame metadata".into(),
            ));
        }
    };

    let samples = width
        .checked_mul(height)
        .and_then(|value| value.checked_mul(4))
        .ok_or(WeaverError::FailedToAllocateMemory(isize::MAX as u64))?;

    if bit_depth.bits() <= 8 {
        let mut pixels = try_vec![0u8; samples];
        let stride = width * 4;
        let mut output = [JxlOutputBuffer::new(&mut pixels, height, stride)];
        match decoder
            .process(&mut input, &mut output)
            .map_err(decode_error)?
        {
            ProcessingResult::Complete { .. } => {}
            ProcessingResult::NeedsMoreInput { .. } => {
                return Err(WeaverError::FailedToDecodeJxl(
                    "truncated while decoding pixels".into(),
                ));
            }
        }
        Ok(PackedJxl::Regular(DecodedJxlPacket {
            data: pixels,
            width,
            height,
            icc,
            bit_depth,
            has_real_alpha,
        }))
    } else {
        let mut pixels = try_vec![0u16; samples];
        let stride = width * 4 * size_of::<u16>();
        let mut output = [JxlOutputBuffer::new(
            bytemuck::cast_slice_mut(&mut pixels),
            height,
            stride,
        )];
        match decoder
            .process(&mut input, &mut output)
            .map_err(decode_error)?
        {
            ProcessingResult::Complete { .. } => {}
            ProcessingResult::NeedsMoreInput { .. } => {
                return Err(WeaverError::FailedToDecodeJxl(
                    "truncated while decoding pixels".into(),
                ));
            }
        }
        Ok(PackedJxl::HighBitDepth(DecodedJxlPacket {
            data: pixels,
            width,
            height,
            icc,
            bit_depth,
            has_real_alpha,
        }))
    }
}

pub(crate) fn read_jxl_info(data: &[u8]) -> Result<(usize, usize, u32), WeaverError> {
    if !is_jxl(data) {
        return Err(WeaverError::InvalidJxl);
    }

    let mut input = data;
    let decoder = match JxlDecoder::new(JxlDecoderOptions::default())
        .process(&mut input)
        .map_err(decode_error)?
    {
        ProcessingResult::Complete { result } => result,
        ProcessingResult::NeedsMoreInput { .. } => {
            return Err(WeaverError::FailedToDecodeJxl(
                "truncated before image metadata".into(),
            ));
        }
    };
    let info = decoder.basic_info();
    Ok((info.size.0, info.size.1, info.bit_depth.bits_per_sample()))
}

#[cfg(test)]
mod tests {
    use super::is_jxl;

    #[test]
    fn recognizes_both_jxl_signatures() {
        assert!(is_jxl(&[0xff, 0x0a]));
        assert!(is_jxl(&[
            0, 0, 0, 0x0c, b'J', b'X', b'L', b' ', 0x0d, 0x0a, 0x87, 0x0a,
        ]));
    }

    #[test]
    fn rejects_truncated_and_non_jxl_inputs() {
        assert!(!is_jxl(&[]));
        assert!(!is_jxl(&[0xff]));
        assert!(!is_jxl(b"not a JPEG XL image"));
    }
}
