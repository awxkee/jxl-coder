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

use crate::jxl_animation::JxlAnimationCoordinator;
use crate::jxl_decode::PackedJxl;
use crate::jxl_decode_android::packed_jxl_to_bitmap;
use crate::support::{dbg_log, init_logging, throw_runtime_exception};
use crate::{WeaveScaleMode, WeaverPreferredColorConfig};
use jni::objects::JObject;
use jni::sys::jobject;
use jni::{EnvUnowned, Outcome};
use std::ptr::null_mut;

/// Decodes an animation frame and returns a ready-to-use `android.graphics.Bitmap`.
///
/// A Java `RuntimeException` is pending and null is returned if decoding or bitmap
/// creation fails. The returned JNI local reference is owned by the calling frame.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn jxl_animation_coordinator_get_frame_bitmap(
    env: *mut jni::sys::JNIEnv,
    coordinator: *const JxlAnimationCoordinator,
    index: u32,
    scaled_width: i32,
    scaled_height: i32,
    scale_mode: WeaveScaleMode,
    preferred_color_config: WeaverPreferredColorConfig,
) -> jobject {
    if env.is_null() {
        return null_mut();
    }

    init_logging();
    let mut unowned = unsafe { EnvUnowned::from_raw(env) };
    let outcome = unowned.with_env(|env| -> Result<jobject, jni::errors::Error> {
        if coordinator.is_null() {
            throw_runtime_exception(env, "JPEG XL animation coordinator pointer is null");
            return Ok(JObject::null().as_raw());
        }

        let result = (|| -> Result<jobject, anyhow::Error> {
            let packet = unsafe { (&*coordinator).decode_frame_packet(index) }
                .map_err(anyhow::Error::msg)?;
            let bitmap = packed_jxl_to_bitmap(
                env,
                PackedJxl::Regular(packet),
                scaled_width,
                scaled_height,
                scale_mode,
                preferred_color_config,
            )?;
            Ok(bitmap.as_raw())
        })();

        match result {
            Ok(bitmap) => Ok(bitmap),
            Err(error) => {
                dbg_log!(
                    error,
                    "jxl_animation_coordinator_get_frame_bitmap failed: {error:#}"
                );
                throw_runtime_exception(env, error.to_string());
                Ok(JObject::null().as_raw())
            }
        }
    });

    match outcome.into_outcome() {
        Outcome::Ok(bitmap) => bitmap,
        Outcome::Err(_error) => {
            dbg_log!(
                error,
                "JNI error while creating animation bitmap: {_error:?}"
            );
            null_mut()
        }
        Outcome::Panic(_panic) => {
            dbg_log!(error, "Rust panic while creating animation bitmap");
            null_mut()
        }
    }
}
