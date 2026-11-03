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
#[cfg(all(
    target_os = "android",
    any(target_arch = "aarch64", target_arch = "arm")
))]
use jni::{jni_sig, objects::JObject, JValue};
#[cfg(target_os = "android")]
use jni::{jni_str, strings::JNIString, sys::jobject, Env, EnvUnowned, Outcome};
#[cfg(target_os = "android")]
use std::sync::OnceLock;

#[cfg(any(target_os = "android", test))]
macro_rules! try_vec {
    () => {
        Vec::new()
    };
    ($elem:expr; $n:expr) => {{
        let mut v = Vec::new();
        v.try_reserve_exact($n)
            .map_err(|_| WeaverError::FailedToAllocateMemory($n as u64))?;
        v.resize($n, $elem);
        v
    }};
}

#[cfg(target_os = "android")]
use crate::WeaverPreferredColorConfig;
#[cfg(any(target_os = "android", test))]
pub(crate) use try_vec;

#[cfg(target_os = "android")]
pub(crate) const MIN_OS_F16: i32 = 26;
#[cfg(target_os = "android")]
pub(crate) const MIN_OS_AR30: i32 = 33;

#[cfg(target_os = "android")]
pub(crate) struct PackedImageBuffer {
    pub(crate) data: Vec<u8>,
    pub(crate) width: usize,
    pub(crate) height: usize,
    pub(crate) format: WeaverPreferredColorConfig,
}

#[cfg(target_os = "android")]
pub(crate) enum PackedImageTransfer {
    Image(PackedImageBuffer),
    HardwareBuffer(jobject),
}

#[cfg(target_os = "android")]
static SDK_VERSION: OnceLock<i32> = OnceLock::new();

#[cfg(target_os = "android")]
pub(crate) fn android_os_version() -> i32 {
    *SDK_VERSION.get_or_init(|| {
        let key = b"ro.build.version.sdk\0";
        let mut buf = [0u8; 92]; // PROP_VALUE_MAX

        unsafe extern "C" {
            fn __system_property_get(
                name: *const std::ffi::c_char,
                value: *mut std::ffi::c_char,
            ) -> std::ffi::c_int;
        }

        let len = unsafe { __system_property_get(key.as_ptr().cast(), buf.as_mut_ptr().cast()) };

        if len <= 0 {
            return 0;
        }

        std::ffi::CStr::from_bytes_until_nul(&buf)
            .ok()
            .and_then(|s| s.to_str().ok())
            .and_then(|s| s.parse().ok())
            .unwrap_or(0)
    })
}

#[cfg(target_os = "android")]
macro_rules! dbg_log {
    ($level:ident, $($arg:tt)*) => {
        #[cfg(feature = "logging")]
        log::$level!(target: "avifweaver", $($arg)*);
    };
}
#[cfg(target_os = "android")]
pub(crate) use dbg_log;

#[cfg(all(target_os = "android", feature = "logging"))]
static LOGGER: OnceLock<()> = OnceLock::new();

#[cfg(target_os = "android")]
pub(crate) fn init_logging() {
    #[cfg(feature = "logging")]
    LOGGER.get_or_init(|| {
        android_logger::init_once(
            android_logger::Config::default()
                .with_max_level(log::LevelFilter::Debug)
                .with_tag("avifweaver"),
        );
    });
}

#[inline]
#[cfg(target_os = "android")]
pub(crate) fn panic_payload_to_string(payload: &(dyn std::any::Any + Send + 'static)) -> String {
    payload
        .downcast_ref::<&str>()
        .map(|s| (*s).to_string())
        .or_else(|| payload.downcast_ref::<String>().cloned())
        .unwrap_or_else(|| "unknown panic".to_string())
}

#[cfg(target_os = "android")]
pub(crate) fn throw_runtime_exception(env: &mut Env, message: impl Into<String>) {
    let message = message.into();

    if env.exception_check() {
        dbg_log!(
            error,
            "not throwing RuntimeException because a Java exception is already pending: {message}"
        );
        return;
    }

    if let Err(_e) = env.throw_new(
        jni_str!("java/lang/RuntimeException"),
        JNIString::from(message.clone()),
    ) {
        dbg_log!(
            error,
            "failed to throw Java RuntimeException: {_e}; original error: {message}"
        );
    }
}

#[cfg(target_os = "android")]
pub(crate) unsafe fn throw_runtime_exception_raw(
    env: *mut jni::sys::JNIEnv,
    message: impl Into<String>,
) {
    if env.is_null() {
        return;
    }

    let message = message.into();
    let mut unowned = unsafe { EnvUnowned::from_raw(env) };
    let outcome = unowned.with_env(move |env| -> Result<(), jni::errors::Error> {
        throw_runtime_exception(env, message);
        Ok(())
    });

    match outcome.into_outcome() {
        Outcome::Ok(()) => {}
        Outcome::Err(_e) => {
            dbg_log!(
                error,
                "failed to attach Env while throwing Java exception: {_e}"
            );
        }
        Outcome::Panic(panic) => {
            let _msg = panic_payload_to_string(panic.as_ref());
            dbg_log!(error, "panic while throwing Java exception: {_msg}");
        }
    }
}

#[cfg(all(
    target_os = "android",
    any(target_arch = "aarch64", target_arch = "arm")
))]
pub(crate) fn optional_bytebuffer_to_vec(
    env: &mut Env,
    obj: jobject,
) -> Result<Option<Vec<u8>>, jni::errors::Error> {
    if obj.is_null() {
        return Ok(None);
    }

    let buf = unsafe { JObject::from_raw(env, obj) };

    let remaining = env
        .call_method(&buf, jni_str!("remaining"), jni_sig!("()I"), &[])?
        .i()? as usize;

    if remaining == 0 {
        return Ok(Some(Vec::new()));
    }

    let dup = env
        .call_method(
            &buf,
            jni_str!("duplicate"),
            jni_sig!("()Ljava/nio/ByteBuffer;"),
            &[],
        )?
        .l()?;

    let arr = env.new_byte_array(remaining)?;
    env.call_method(
        &dup,
        jni_str!("get"),
        jni_sig!("([B)Ljava/nio/ByteBuffer;"),
        &[JValue::Object(&arr)],
    )?;

    Ok(Some(env.convert_byte_array(&arr)?))
}
