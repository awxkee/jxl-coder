/*
 * Copyright (c) Radzivon Bartoshyk 2026/7. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
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

use crate::jxl_decode::{
    is_jxl, limited_decoder_options, validate_jxl_dimensions, BitDepth, DecodedJxlPacket,
};
use crate::jxl_thread_runner::JxlThreadRunner;
use jxl::api::{
    Event, ExtraChannel, JxlColorType, JxlDataFormat, JxlDecoder, JxlOutputBuffer, JxlPixelFormat,
    VisibleFrameInfo,
};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr::null_mut;
use std::slice;

/// Opaque Rust-owned decoder state. C++ must only use pointers to this type.
pub struct JxlAnimationCoordinator {
    data: Vec<u8>,
    frames: Vec<VisibleFrameInfo>,
    width: usize,
    height: usize,
    source_bit_depth: u32,
    loop_count: i32,
    has_alpha: bool,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum JxlAnimationStatus {
    Ok = 0,
    NullPointer = 1,
    InvalidJxl = 2,
    TruncatedInput = 3,
    FrameOutOfRange = 4,
    AllocationFailed = 5,
    DecodeFailed = 6,
    Panic = 7,
}

#[repr(C)]
pub struct JxlAnimationBuffer {
    pub data: *mut u8,
    pub length: usize,
    pub capacity: usize,
}

impl JxlAnimationBuffer {
    fn empty() -> Self {
        Self {
            data: null_mut(),
            length: 0,
            capacity: 0,
        }
    }

    fn from_vec(mut data: Vec<u8>) -> Self {
        let buffer = Self {
            data: data.as_mut_ptr(),
            length: data.len(),
            capacity: data.capacity(),
        };
        std::mem::forget(data);
        buffer
    }
}

#[repr(C)]
pub struct JxlAnimationCreateResult {
    pub coordinator: *mut JxlAnimationCoordinator,
    pub status: JxlAnimationStatus,
    /// UTF-8 bytes; not NUL-terminated. Release with `jxl_animation_buffer_release`.
    pub error: JxlAnimationBuffer,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct JxlAnimationInfo {
    pub width: u32,
    pub height: u32,
    pub source_bit_depth: u32,
    pub frame_count: u32,
    /// JPEG XL semantics: 0 means infinite. Non-animated images report -1.
    pub loop_count: i32,
    pub has_alpha: bool,
}

impl JxlAnimationInfo {
    fn empty() -> Self {
        Self {
            width: 0,
            height: 0,
            source_bit_depth: 0,
            frame_count: 0,
            loop_count: -1,
            has_alpha: false,
        }
    }
}

#[repr(C)]
pub struct JxlAnimationFrame {
    /// Tightly packed RGBA8 pixels with `stride == width * 4`.
    pub pixels: JxlAnimationBuffer,
    /// ICC profile describing `pixels`; may be empty.
    pub icc: JxlAnimationBuffer,
    pub width: u32,
    pub height: u32,
    pub stride: u32,
    pub duration_ms: u32,
    pub has_alpha: bool,
    /// jxl-rs currently emits straight alpha for this API.
    pub alpha_premultiplied: bool,
}

impl JxlAnimationFrame {
    fn empty() -> Self {
        Self {
            pixels: JxlAnimationBuffer::empty(),
            icc: JxlAnimationBuffer::empty(),
            width: 0,
            height: 0,
            stride: 0,
            duration_ms: 0,
            has_alpha: false,
            alpha_premultiplied: false,
        }
    }
}

#[repr(C)]
pub struct JxlAnimationFrameResult {
    pub frame: JxlAnimationFrame,
    pub status: JxlAnimationStatus,
    /// UTF-8 bytes; not NUL-terminated. Release with `jxl_animation_buffer_release`.
    pub error: JxlAnimationBuffer,
}

struct AnimationError {
    status: JxlAnimationStatus,
    message: String,
}

impl AnimationError {
    fn new(status: JxlAnimationStatus, message: impl Into<String>) -> Self {
        Self {
            status,
            message: message.into(),
        }
    }

    fn decode(error: impl std::fmt::Display) -> Self {
        Self::new(JxlAnimationStatus::DecodeFailed, error.to_string())
    }
}

fn complete(
    result: Result<Event, jxl::error::Error>,
    expected: Event,
    stage: &str,
) -> Result<Event, AnimationError> {
    match result.map_err(AnimationError::decode)? {
        Event::NeedMoreInput { .. } => Err(AnimationError::new(
            JxlAnimationStatus::TruncatedInput,
            format!("JPEG XL input ended while reading {stage}"),
        )),
        event
            if event == expected
                || matches!(
                    (event, expected),
                    (Event::FrameComplete { .. }, Event::FrameComplete { .. })
                ) =>
        {
            Ok(event)
        }
        event => Err(AnimationError::decode(format!(
            "Expected {expected:?} while reading {stage}, received {event:?}"
        ))),
    }
}

fn allocate_zeroed(length: usize) -> Result<Vec<u8>, AnimationError> {
    let mut data = Vec::new();
    data.try_reserve_exact(length).map_err(|_| {
        AnimationError::new(
            JxlAnimationStatus::AllocationFailed,
            format!("Failed to allocate {length} bytes"),
        )
    })?;
    data.resize(length, 0);
    Ok(data)
}

impl JxlAnimationCoordinator {
    fn create(data: &[u8]) -> Result<Box<Self>, AnimationError> {
        if !is_jxl(data) {
            return Err(AnimationError::new(
                JxlAnimationStatus::InvalidJxl,
                "Input is not a JPEG XL image",
            ));
        }

        let mut owned = Vec::new();
        owned.try_reserve_exact(data.len()).map_err(|_| {
            AnimationError::new(
                JxlAnimationStatus::AllocationFailed,
                format!("Failed to allocate {} input bytes", data.len()),
            )
        })?;
        owned.extend_from_slice(data);

        let mut options = limited_decoder_options();
        options.scan_frames_only = true;
        let mut input = owned.as_slice();
        let mut decoder = JxlDecoder::new(options);
        complete(
            decoder.process(&mut input, None, None),
            Event::BasicInfo,
            "image metadata",
        )?;

        let basic = decoder
            .basic_info()
            .ok_or_else(|| AnimationError::decode("Missing image metadata"))?
            .clone();
        let (width, height) = basic.size;
        validate_jxl_dimensions(width, height).map_err(AnimationError::decode)?;
        let pixels = width
            .checked_mul(height)
            .and_then(|value| value.checked_mul(4))
            .ok_or_else(|| {
                AnimationError::new(
                    JxlAnimationStatus::AllocationFailed,
                    "JPEG XL image dimensions overflow the address space",
                )
            })?;
        if pixels > i32::MAX as usize {
            return Err(AnimationError::new(
                JxlAnimationStatus::AllocationFailed,
                "JPEG XL image exceeds the Android pixel limit",
            ));
        }

        loop {
            complete(
                decoder.process(&mut input, None, None),
                Event::FrameHeader,
                "frame metadata",
            )?;
            let event = complete(
                decoder.process(&mut input, None, None),
                Event::FrameComplete {
                    has_more_frames: false,
                },
                "frame data",
            )?;
            if event
                == (Event::FrameComplete {
                    has_more_frames: false,
                })
            {
                break;
            }
        }

        let frames = decoder.scanned_frames().to_vec();
        if frames.is_empty() {
            return Err(AnimationError::new(
                JxlAnimationStatus::DecodeFailed,
                "JPEG XL image contains no visible frames",
            ));
        }

        let has_alpha = basic
            .extra_channels
            .iter()
            .any(|channel| channel.ec_type == ExtraChannel::Alpha);
        let loop_count = basic
            .animation
            .as_ref()
            .map_or(-1, |animation| animation.num_loops as i32);

        Ok(Box::new(Self {
            data: owned,
            frames,
            width,
            height,
            source_bit_depth: basic.bit_depth.bits_per_sample(),
            loop_count,
            has_alpha,
        }))
    }

    fn info(&self) -> Result<JxlAnimationInfo, AnimationError> {
        Ok(JxlAnimationInfo {
            width: u32::try_from(self.width).map_err(|_| {
                AnimationError::new(JxlAnimationStatus::DecodeFailed, "Width exceeds u32")
            })?,
            height: u32::try_from(self.height).map_err(|_| {
                AnimationError::new(JxlAnimationStatus::DecodeFailed, "Height exceeds u32")
            })?,
            source_bit_depth: self.source_bit_depth,
            frame_count: u32::try_from(self.frames.len()).map_err(|_| {
                AnimationError::new(JxlAnimationStatus::DecodeFailed, "Frame count exceeds u32")
            })?,
            loop_count: self.loop_count,
            has_alpha: self.has_alpha,
        })
    }

    fn frame_duration(&self, index: u32) -> Result<u32, AnimationError> {
        let frame = self.frames.get(index as usize).ok_or_else(|| {
            AnimationError::new(
                JxlAnimationStatus::FrameOutOfRange,
                format!(
                    "Frame index {index} is outside the available range 0..{}",
                    self.frames.len()
                ),
            )
        })?;
        Ok(frame.duration_ms.round().clamp(0.0, u32::MAX as f64) as u32)
    }

    pub(crate) fn decode_frame_packet(&self, index: u32) -> Result<DecodedJxlPacket<u8>, String> {
        self.decode_frame_packet_inner(index)
            .map_err(|error| error.message)
    }

    fn decode_frame_packet_inner(
        &self,
        index: u32,
    ) -> Result<DecodedJxlPacket<u8>, AnimationError> {
        self.frames.get(index as usize).ok_or_else(|| {
            AnimationError::new(
                JxlAnimationStatus::FrameOutOfRange,
                format!(
                    "Frame index {index} is outside the available range 0..{}",
                    self.frames.len()
                ),
            )
        })?;

        let mut runner = JxlThreadRunner::default();
        let mut initial_input = self.data.as_slice();
        let options = limited_decoder_options();
        let mut decoder = JxlDecoder::new(options);
        complete(
            decoder.process(&mut initial_input, None, Some(&mut runner)),
            Event::BasicInfo,
            "image metadata",
        )?;
        decoder
            .set_pixel_format(JxlPixelFormat {
                color_type: JxlColorType::Rgba,
                color_data_format: Some(JxlDataFormat::U8 { bit_depth: 8 }),
                extra_channel_format: vec![
                    None;
                    decoder
                        .basic_info()
                        .ok_or_else(|| AnimationError::decode("Missing image metadata"))?
                        .extra_channels
                        .len()
                ],
            })
            .map_err(AnimationError::decode)?;
        let icc = decoder
            .output_color_profile()
            .and_then(|profile| profile.try_as_icc())
            .map_or_else(Vec::new, |profile| profile.into_owned());

        // Read preceding frames without producing output so their reference
        // data remains available. This also handles boxed streams whose first
        // frame begins in the middle of a jxlp box.
        for _ in 0..index {
            complete(
                decoder.process(&mut initial_input, None, Some(&mut runner)),
                Event::FrameHeader,
                "frame metadata",
            )?;
            complete(
                decoder.process(&mut initial_input, None, Some(&mut runner)),
                Event::FrameComplete {
                    has_more_frames: true,
                },
                "frame data",
            )?;
        }
        complete(
            decoder.process(&mut initial_input, None, Some(&mut runner)),
            Event::FrameHeader,
            "frame metadata",
        )?;

        let stride = self.width.checked_mul(4).ok_or_else(|| {
            AnimationError::new(
                JxlAnimationStatus::AllocationFailed,
                "Frame stride overflow",
            )
        })?;
        let length = stride.checked_mul(self.height).ok_or_else(|| {
            AnimationError::new(JxlAnimationStatus::AllocationFailed, "Frame size overflow")
        })?;
        let mut pixels = allocate_zeroed(length)?;
        let mut output = [JxlOutputBuffer::new(
            pixels.as_mut_slice(),
            self.height,
            stride,
        )];
        complete(
            decoder.process(&mut initial_input, Some(&mut output), Some(&mut runner)),
            Event::FrameComplete {
                has_more_frames: false,
            },
            "frame pixels",
        )?;

        Ok(DecodedJxlPacket {
            data: pixels,
            width: self.width,
            height: self.height,
            icc: (!icc.is_empty()).then_some(icc),
            bit_depth: BitDepth::new(8).map_err(AnimationError::decode)?,
            has_real_alpha: self.has_alpha,
        })
    }

    fn decode_frame(&self, index: u32) -> Result<JxlAnimationFrame, AnimationError> {
        let packet = self.decode_frame_packet_inner(index)?;
        let stride = packet.width.checked_mul(4).ok_or_else(|| {
            AnimationError::new(
                JxlAnimationStatus::AllocationFailed,
                "Frame stride overflow",
            )
        })?;
        Ok(JxlAnimationFrame {
            pixels: JxlAnimationBuffer::from_vec(packet.data),
            icc: JxlAnimationBuffer::from_vec(packet.icc.unwrap_or_default()),
            width: packet.width as u32,
            height: packet.height as u32,
            stride: stride as u32,
            duration_ms: self.frame_duration(index)?,
            has_alpha: packet.has_real_alpha,
            alpha_premultiplied: false,
        })
    }
}

fn error_buffer(message: String) -> JxlAnimationBuffer {
    JxlAnimationBuffer::from_vec(message.into_bytes())
}

fn create_error(error: AnimationError) -> JxlAnimationCreateResult {
    JxlAnimationCreateResult {
        coordinator: null_mut(),
        status: error.status,
        error: error_buffer(error.message),
    }
}

fn frame_error(error: AnimationError) -> JxlAnimationFrameResult {
    JxlAnimationFrameResult {
        frame: JxlAnimationFrame::empty(),
        status: error.status,
        error: error_buffer(error.message),
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn jxl_animation_coordinator_create(
    data: *const u8,
    length: usize,
) -> JxlAnimationCreateResult {
    if data.is_null() {
        return create_error(AnimationError::new(
            JxlAnimationStatus::NullPointer,
            "JPEG XL input pointer is null",
        ));
    }
    let result = catch_unwind(AssertUnwindSafe(|| {
        let bytes = if length == 0 {
            &[]
        } else {
            unsafe { slice::from_raw_parts(data, length) }
        };
        JxlAnimationCoordinator::create(bytes)
    }));
    match result {
        Ok(Ok(coordinator)) => JxlAnimationCreateResult {
            coordinator: Box::into_raw(coordinator),
            status: JxlAnimationStatus::Ok,
            error: JxlAnimationBuffer::empty(),
        },
        Ok(Err(error)) => create_error(error),
        Err(_) => create_error(AnimationError::new(
            JxlAnimationStatus::Panic,
            "Rust panicked while creating the JPEG XL animation coordinator",
        )),
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn jxl_animation_coordinator_destroy(
    coordinator: *mut JxlAnimationCoordinator,
) {
    if !coordinator.is_null() {
        let _ = catch_unwind(AssertUnwindSafe(|| unsafe {
            drop(Box::from_raw(coordinator));
        }));
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn jxl_animation_coordinator_get_info(
    coordinator: *const JxlAnimationCoordinator,
    info: *mut JxlAnimationInfo,
) -> JxlAnimationStatus {
    if coordinator.is_null() || info.is_null() {
        return JxlAnimationStatus::NullPointer;
    }
    match catch_unwind(AssertUnwindSafe(|| unsafe { (&*coordinator).info() })) {
        Ok(Ok(value)) => {
            unsafe { info.write(value) };
            JxlAnimationStatus::Ok
        }
        Ok(Err(error)) => error.status,
        Err(_) => {
            unsafe { info.write(JxlAnimationInfo::empty()) };
            JxlAnimationStatus::Panic
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn jxl_animation_coordinator_get_frame_duration(
    coordinator: *const JxlAnimationCoordinator,
    index: u32,
    duration_ms: *mut u32,
) -> JxlAnimationStatus {
    if coordinator.is_null() || duration_ms.is_null() {
        return JxlAnimationStatus::NullPointer;
    }
    match catch_unwind(AssertUnwindSafe(|| unsafe {
        (&*coordinator).frame_duration(index)
    })) {
        Ok(Ok(value)) => {
            unsafe { duration_ms.write(value) };
            JxlAnimationStatus::Ok
        }
        Ok(Err(error)) => error.status,
        Err(_) => JxlAnimationStatus::Panic,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn jxl_animation_coordinator_decode_frame(
    coordinator: *const JxlAnimationCoordinator,
    index: u32,
) -> JxlAnimationFrameResult {
    if coordinator.is_null() {
        return frame_error(AnimationError::new(
            JxlAnimationStatus::NullPointer,
            "JPEG XL animation coordinator pointer is null",
        ));
    }
    match catch_unwind(AssertUnwindSafe(|| unsafe {
        (&*coordinator).decode_frame(index)
    })) {
        Ok(Ok(frame)) => JxlAnimationFrameResult {
            frame,
            status: JxlAnimationStatus::Ok,
            error: JxlAnimationBuffer::empty(),
        },
        Ok(Err(error)) => frame_error(error),
        Err(_) => frame_error(AnimationError::new(
            JxlAnimationStatus::Panic,
            "Rust panicked while decoding a JPEG XL animation frame",
        )),
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn jxl_animation_buffer_release(buffer: JxlAnimationBuffer) {
    if !buffer.data.is_null() {
        let _ = catch_unwind(AssertUnwindSafe(|| unsafe {
            drop(Vec::from_raw_parts(
                buffer.data,
                buffer.length,
                buffer.capacity,
            ));
        }));
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn jxl_animation_frame_release(frame: JxlAnimationFrame) {
    unsafe {
        jxl_animation_buffer_release(frame.pixels);
        jxl_animation_buffer_release(frame.icc);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::jxl_decode::{decode_packed_jxl, PackedJxl};
    use std::ptr::null;

    #[test]
    fn tall_frame_matches_lossless_source_and_still_decoder() {
        let data = include_bytes!("../../jxlcoder/src/androidTest/assets/tall-pattern.jxl");
        let coordinator = JxlAnimationCoordinator::create(data)
            .unwrap_or_else(|error| panic!("{}", error.message));
        let frame = coordinator.decode_frame_packet(0).unwrap();
        assert_eq!((frame.width, frame.height), (32, 512));
        assert_eq!(coordinator.frames.len(), 1);
        for y in 0..512 {
            for x in 0..32 {
                let offset = (y * 32 + x) * 4;
                assert_eq!(
                    &frame.data[offset..offset + 4],
                    &[
                        if (x / 4 + y / 4) % 2 != 0 { 255 } else { 0 },
                        (y % 256) as u8,
                        (x * 8) as u8,
                        255
                    ],
                );
            }
        }
        let PackedJxl::Regular(still) = decode_packed_jxl(data).unwrap() else {
            panic!("expected an 8-bit image");
        };
        assert_eq!(frame.data, still.data);
    }

    #[test]
    fn decodes_animation_frames_in_arbitrary_order() {
        let data = include_bytes!("../../app/src/main/assets/animated_jxl.jxl");
        let coordinator = JxlAnimationCoordinator::create(data)
            .unwrap_or_else(|error| panic!("{}", error.message));
        let count = coordinator.frames.len();
        assert!(count > 1);
        let first = coordinator.decode_frame_packet(0).unwrap();
        for index in [count - 1, count / 2, 0] {
            let frame = coordinator.decode_frame_packet(index as u32).unwrap();
            assert_eq!((frame.width, frame.height), (first.width, first.height));
            assert_eq!(frame.data.len(), first.data.len());
            if index == 0 {
                assert_eq!(frame.data, first.data);
            }
        }
        let error = coordinator
            .decode_frame_packet_inner(count as u32)
            .err()
            .unwrap();
        assert_eq!(error.status, JxlAnimationStatus::FrameOutOfRange);
    }

    #[test]
    fn truncated_animation_is_reported_without_unwinding() {
        let data = include_bytes!("../../app/src/main/assets/animated_jxl.jxl");
        let error = JxlAnimationCoordinator::create(&data[..data.len() / 2])
            .err()
            .unwrap();
        assert_eq!(error.status, JxlAnimationStatus::TruncatedInput);
    }

    #[test]
    fn null_create_is_reported_without_unwinding() {
        let result = unsafe { jxl_animation_coordinator_create(null(), 0) };
        assert_eq!(result.status, JxlAnimationStatus::NullPointer);
        unsafe { jxl_animation_buffer_release(result.error) };
    }

    #[test]
    fn invalid_input_is_reported_without_unwinding() {
        let input = b"not a JPEG XL image";
        let result = unsafe { jxl_animation_coordinator_create(input.as_ptr(), input.len()) };
        assert_eq!(result.status, JxlAnimationStatus::InvalidJxl);
        unsafe { jxl_animation_buffer_release(result.error) };
    }
}
