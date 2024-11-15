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

use crate::scaling_function::ScalingFunction;
use pic_scale::{
    ImageSize, ImageStore, LinearApproxScaler, Scaler, Scaling, ScalingU16, ThreadingPolicy,
};
use std::slice;

#[no_mangle]
pub unsafe extern "C" fn weave_scale_u8(
    src: *const u8,
    src_stride: u32,
    width: u32,
    height: u32,
    dst: *const u8,
    dst_stride: u32,
    new_width: u32,
    new_height: u32,
    scaling_function: ScalingFunction,
    premultiply_alpha: bool,
) {
    let mut src_slice = vec![0u8; width as usize * height as usize * 4];
    let c_source_slice = slice::from_raw_parts(src, src_stride as usize * height as usize);

    for (dst, src) in src_slice
        .chunks_exact_mut(width as usize * 4)
        .zip(c_source_slice.chunks_exact(src_stride as usize))
    {
        for (dst, src) in dst.iter_mut().zip(src.iter()) {
            *dst = *src;
        }
    }

    let source_store =
        ImageStore::<u8, 4>::new(src_slice, width as usize, height as usize).unwrap();

    let mut scaler = LinearApproxScaler::new(scaling_function.to_resampling_function());

    scaler.set_threading_policy(ThreadingPolicy::Adaptive);
    let dst_slice = unsafe {
        slice::from_raw_parts_mut(dst as *mut u8, dst_stride as usize * new_height as usize)
    };

    let new_store = scaler
        .resize_rgba(
            ImageSize::new(new_width as usize, new_height as usize),
            source_store,
            premultiply_alpha,
        )
        .unwrap();

    for (dst_chunk, src_chunk) in dst_slice
        .chunks_mut(dst_stride as usize)
        .zip(new_store.as_bytes().chunks(new_width as usize * 4))
    {
        for (dst, src) in dst_chunk.iter_mut().zip(src_chunk.iter()) {
            *dst = *src;
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn weave_scale_u16(
    src: *const u16,
    src_stride: usize,
    width: u32,
    height: u32,
    dst: *mut u16,
    dst_stride: usize,
    new_width: u32,
    new_height: u32,
    bit_depth: usize,
    scaling_function: ScalingFunction,
    premultiply_alpha: bool,
) {
    let mut _src_slice = vec![0u16; width as usize * height as usize * 4];

    let source_image = slice::from_raw_parts(src as *const u8, src_stride * height as usize);

    for (dst, src) in _src_slice
        .chunks_exact_mut(width as usize * 4)
        .zip(source_image.chunks_exact(src_stride))
    {
        for (dst, src) in dst.iter_mut().zip(src.chunks_exact(2)) {
            let pixel = u16::from_ne_bytes([src[0], src[1]]);
            *dst = pixel;
        }
    }

    let _source_store =
        ImageStore::<u16, 4>::new(_src_slice, width as usize, height as usize).unwrap();

    let mut scaler = Scaler::new(scaling_function.to_resampling_function());
    scaler.set_threading_policy(ThreadingPolicy::Adaptive);

    let _new_store = scaler
        .resize_rgba_u16(
            ImageSize::new(new_width as usize, new_height as usize),
            _source_store,
            bit_depth,
            premultiply_alpha,
        )
        .unwrap();

    let dst_slice =
        unsafe { slice::from_raw_parts_mut(dst as *mut u8, dst_stride * new_height as usize) };
    for (dst, src) in dst_slice
        .chunks_exact_mut(dst_stride)
        .zip(_new_store.as_bytes().chunks_exact(4 * _new_store.width))
    {
        for (dst, src) in dst.chunks_exact_mut(2).zip(src.iter()) {
            let bytes = src.to_ne_bytes();
            dst[0] = bytes[0];
            dst[1] = bytes[1];
        }
    }
}
