/*
 * Copyright (c) Radzivon Bartoshyk 2026/6. All rights reserved.
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

use std::slice;

/// JPEG XL may be represented as either a bare codestream or a box container.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub enum ImageContainer {
    Unknown = 0,
    JxlCodestream = 1,
    JxlContainer = 2,
}

/// Bare JPEG XL codestream marker: FF 0A.
static JXL_CODESTREAM_SIGNATURE: [u8; 2] = [0xFF, 0x0A];

/// JPEG XL container signature box:
///
/// 00 00 00 0C  J X L SP  0D 0A 87 0A
static JXL_CONTAINER_SIGNATURE: [u8; 12] = [
    0x00, 0x00, 0x00, 0x0C, b'J', b'X', b'L', b' ', 0x0D, 0x0A, 0x87, 0x0A,
];

#[inline]
fn detect_container(bytes: &[u8]) -> ImageContainer {
    if bytes.starts_with(&JXL_CODESTREAM_SIGNATURE) {
        ImageContainer::JxlCodestream
    } else if bytes.starts_with(&JXL_CONTAINER_SIGNATURE) {
        ImageContainer::JxlContainer
    } else {
        ImageContainer::Unknown
    }
}

#[inline]
unsafe fn detect_image_container(data: *const u8, len: usize) -> ImageContainer {
    if data.is_null() || len < JXL_CODESTREAM_SIGNATURE.len() {
        return ImageContainer::Unknown;
    }

    // SAFETY:
    // The caller guarantees that `data` points to at least `len`
    // readable bytes. Null pointers are rejected above.
    let bytes = unsafe { slice::from_raw_parts(data, len) };

    detect_container(bytes)
}

/// Returns true for both bare JPEG XL codestreams and JXL containers.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn is_jxl_image(data: *const u8, len: usize) -> bool {
    matches!(
        unsafe { detect_image_container(data, len) },
        ImageContainer::JxlCodestream | ImageContainer::JxlContainer
    )
}

/// Returns true only for a bare JPEG XL codestream.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn is_jxl_codestream(data: *const u8, len: usize) -> bool {
    (unsafe { detect_image_container(data, len) }) == ImageContainer::JxlCodestream
}

/// Returns true only for a JPEG XL box container.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn is_jxl_container(data: *const u8, len: usize) -> bool {
    (unsafe { detect_image_container(data, len) }) == ImageContainer::JxlContainer
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn container_recognisance(data: *const u8, len: usize) -> ImageContainer {
    unsafe { detect_image_container(data, len) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn detects_bare_jxl_codestream() {
        let data = [0xFF, 0x0A, 0x00, 0x10];

        assert_eq!(detect_container(&data), ImageContainer::JxlCodestream);
    }

    #[test]
    fn detects_jxl_container() {
        assert_eq!(
            detect_container(&JXL_CONTAINER_SIGNATURE),
            ImageContainer::JxlContainer
        );
    }

    #[test]
    fn rejects_truncated_container_signature() {
        assert_eq!(
            detect_container(&JXL_CONTAINER_SIGNATURE[..8]),
            ImageContainer::Unknown
        );
    }

    #[test]
    fn rejects_non_jxl_data() {
        assert_eq!(
            detect_container(b"not a JPEG XL image"),
            ImageContainer::Unknown
        );
    }

    #[test]
    fn rejects_empty_data() {
        assert_eq!(detect_container(&[]), ImageContainer::Unknown);
    }
}
