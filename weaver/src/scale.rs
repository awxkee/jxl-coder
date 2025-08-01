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
use num_traits::FromPrimitive;
use pic_scale::{
    BufferStore, ImageStore, ImageStoreMut, ImageStoreScaling, PicScaleError, ScalingOptions,
    ThreadingPolicy,
};
use std::fmt::Debug;
use std::slice;

#[inline]
fn map_error_code(error: Result<(), PicScaleError>) -> usize {
    match error {
        Ok(_) => 0,
        Err(err) => err.code(),
    }
}

fn pic_scale_scale_generic<
    'a,
    T: Sized + Copy + Clone + Default + Debug + FromPrimitive + 'static,
    const N: usize,
>(
    src: *const T,
    src_stride: usize,
    width: u32,
    height: u32,
    dst: *mut T,
    dst_stride: usize,
    new_width: u32,
    new_height: u32,
    bit_depth: u32,
    resizing_filter: ScalingFunction,
    premultiply_alpha: bool,
) -> usize
where
    ImageStore<'a, T, N>: ImageStoreScaling<'a, T, N>,
{
    unsafe {
        let source_image: std::borrow::Cow<[T]>;

        let required_align_of_t: usize = align_of::<T>();
        let size_of_t: usize = size_of::<T>();

        let mut j_src_stride = src_stride / size_of_t;

        if src as usize % required_align_of_t != 0 || src_stride % size_of_t != 0 {
            let mut _src_slice = vec![T::default(); width as usize * height as usize * N];
            let j = slice::from_raw_parts(src as *const u8, src_stride * height as usize);

            for (dst, src) in _src_slice
                .chunks_exact_mut(width as usize * N)
                .zip(j.chunks_exact(src_stride))
            {
                for (dst, src) in dst.iter_mut().zip(src.chunks_exact(N)) {
                    let src_pixel = src.as_ptr() as *const T;
                    *dst = src_pixel.read_unaligned();
                }
            }
            source_image = std::borrow::Cow::Owned(_src_slice);
            j_src_stride = width as usize * N;
        } else {
            source_image = std::borrow::Cow::Borrowed(slice::from_raw_parts(
                src,
                src_stride / size_of_t * height as usize,
            ));
        }

        let source_store = ImageStore::<T, N> {
            buffer: source_image,
            channels: N,
            width: width as usize,
            height: height as usize,
            stride: j_src_stride,
            bit_depth: bit_depth as usize,
        };

        let mut options = ScalingOptions::default();
        options.premultiply_alpha = premultiply_alpha;
        options.threading_policy = ThreadingPolicy::Single;
        options.resampling_function = resizing_filter.to_resampling_function();

        if dst as usize % required_align_of_t != 0 && dst_stride % size_of_t != 0 {
            let mut dst_store = ImageStoreMut::alloc_with_depth(
                new_width as usize,
                new_height as usize,
                bit_depth as usize,
            );

            let result = source_store.scale(&mut dst_store, options);
            let result_code = map_error_code(result);
            if result_code != 0 {
                return result_code;
            }

            let dst_slice =
                slice::from_raw_parts_mut(dst as *mut u8, new_width as usize * dst_stride);

            for (src, dst) in dst_store
                .as_bytes()
                .chunks_exact(dst_store.stride())
                .zip(dst_slice.chunks_exact_mut(dst_stride))
            {
                for (src, dst) in src.iter().zip(dst.chunks_exact_mut(N)) {
                    let dst_ptr = dst.as_mut_ptr() as *mut T;
                    dst_ptr.write_unaligned(*src);
                }
            }
            0
        } else {
            let dst_slice =
                slice::from_raw_parts_mut(dst, new_height as usize * (dst_stride / size_of_t));
            let buffer = BufferStore::Borrowed(dst_slice);
            let mut dst_store = ImageStoreMut::<T, N> {
                buffer,
                width: new_width as usize,
                height: new_height as usize,
                bit_depth: bit_depth as usize,
                channels: N,
                stride: dst_stride / size_of_t,
            };

            let result = source_store.scale(&mut dst_store, options);
            let result_code = map_error_code(result);
            if result_code != 0 {
                return result_code;
            }
            0
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn weave_scale_u8(
    src: *const u8,
    src_stride: u32,
    width: u32,
    height: u32,
    dst: *mut u8,
    dst_stride: u32,
    new_width: u32,
    new_height: u32,
    scaling_function: ScalingFunction,
    premultiply_alpha: bool,
) -> usize {
    pic_scale_scale_generic::<u8, 4>(
        src,
        src_stride as usize,
        width,
        height,
        dst,
        dst_stride as usize,
        new_width,
        new_height,
        8,
        scaling_function,
        premultiply_alpha,
    )
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
) -> usize {
    pic_scale_scale_generic::<u16, 4>(
        src,
        src_stride,
        width,
        height,
        dst,
        dst_stride,
        new_width,
        new_height,
        bit_depth as u32,
        scaling_function,
        premultiply_alpha,
    )
}
