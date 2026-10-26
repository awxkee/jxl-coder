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

use crate::support::{init_logging, throw_runtime_exception_raw};
use crate::JixelEncodingSpeed;
use jni::sys::{jbyteArray, jobject};
use std::ptr::null_mut;

const SUPPORTED_JIXEL_TARGETS: &str = "aarch64-linux-android and armv7-linux-androideabi";

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
    let message = format!(
        "JPEG XL encoding is not supported on target architecture '{}'. Supported targets: {SUPPORTED_JIXEL_TARGETS}",
        std::env::consts::ARCH,
    );
    unsafe { throw_runtime_exception_raw(env, message) };
    null_mut()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn transcode_jpeg_to_jxl(
    env: *mut jni::sys::JNIEnv,
    _jpeg: *const u8,
    _length: usize,
    _jpeg_reconstruction: bool,
    _num_threads: usize,
) -> jbyteArray {
    init_logging();
    let message = format!(
        "JPEG to JPEG XL transcoding is not supported on target architecture '{}'. Supported targets: {SUPPORTED_JIXEL_TARGETS}",
        std::env::consts::ARCH,
    );
    unsafe { throw_runtime_exception_raw(env, message) };
    null_mut()
}
