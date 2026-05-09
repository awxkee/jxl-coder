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
    BufferStore, ImageStore, ImageStoreMut, ImageStoreScaling, ScalingOptions, ThreadingPolicy,
};
use std::fmt::Debug;
use std::slice;

#[repr(C)]
struct ScalingResultGen<T> {
    data: *mut T,
    width: usize,
    height: usize,
    stride: usize,
    length: usize,
    capacity: usize,
}

#[repr(C)]
pub struct ScalingResultU8 {
    data: *mut u8,
    width: usize,
    height: usize,
    stride: usize,
    length: usize,
    capacity: usize,
}

#[repr(C)]
pub struct ScalingResultU16 {
    data: *mut u16,
    width: usize,
    height: usize,
    stride: usize,
    length: usize,
    capacity: usize,
}

#[repr(C)]
pub enum WeaveScaleMode {
    JustResize,
    ScaleToFill,
    ScaleToFit,
}

#[no_mangle]
pub extern "C" fn weave_scaling_result_free(result: ScalingResultU8) {
    if result.data.is_null() {
        return;
    }
    unsafe {
        _ = Vec::from_raw_parts(result.data, result.length, result.capacity);
    }
}

#[no_mangle]
pub extern "C" fn weave_scaling_result16_free(result: ScalingResultU16) {
    if result.data.is_null() {
        return;
    }
    unsafe {
        _ = Vec::from_raw_parts(result.data, result.length, result.capacity);
    }
}

fn resolve_dimensions(
    src_width: u32,
    src_height: u32,
    new_width: i32,
    new_height: i32,
) -> (usize, usize) {
    match (new_width, new_height) {
        (w, -1) if w > 0 => {
            // auto height
            let scale = w as f64 / src_width as f64;
            let h = (src_height as f64 * scale).round() as usize;
            (w as usize, h.max(1))
        }
        (w, -2) if w > 0 => {
            // auto height divisible by 2
            let scale = w as f64 / src_width as f64;
            let h = (src_height as f64 * scale).round() as usize;
            (w as usize, (h.max(1) + 1) & !1)
        }
        (-1, h) if h > 0 => {
            // auto width
            let scale = h as f64 / src_height as f64;
            let w = (src_width as f64 * scale).round() as usize;
            (w.max(1), h as usize)
        }
        (-2, h) if h > 0 => {
            // auto width divisible by 2
            let scale = h as f64 / src_height as f64;
            let w = (src_width as f64 * scale).round() as usize;
            ((w.max(1) + 1) & !1, h as usize)
        }
        (w, h) => {
            // both explicit
            (w.max(1) as usize, h.max(1) as usize)
        }
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
    new_width: i32,
    new_height: i32,
    bit_depth: u32,
    resizing_filter: ScalingFunction,
    premultiply_alpha: bool,
    scale_mode: WeaveScaleMode,
) -> ScalingResultGen<T>
where
    ImageStore<'a, T, N>: ImageStoreScaling<'a, T, N>,
{
    let source_image: std::borrow::Cow<[T]>;

    let required_align_of_t: usize = align_of::<T>();
    let size_of_t: usize = size_of::<T>();

    let mut j_src_stride = src_stride / size_of_t;

    if src as usize % required_align_of_t != 0 || src_stride % size_of_t != 0 {
        let mut _src_slice = vec![T::default(); width as usize * height as usize * N];
        let inner_source =
            unsafe { slice::from_raw_parts(src as *const u8, src_stride * height as usize) };
        let t_size = size_of::<T>();

        for (dst, src) in _src_slice
            .chunks_exact_mut(width as usize * N)
            .zip(inner_source.chunks_exact(src_stride))
        {
            for (dst, src) in dst.iter_mut().zip(src.chunks_exact(t_size)) {
                let src_pixel = src.as_ptr() as *const T;
                unsafe {
                    *dst = src_pixel.read_unaligned();
                }
            }
        }
        source_image = std::borrow::Cow::Owned(_src_slice);
        j_src_stride = width as usize * N;
    } else {
        unsafe {
            source_image = std::borrow::Cow::Borrowed(slice::from_raw_parts(
                src,
                src_stride / size_of_t * height as usize,
            ));
        }
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

    let (new_width, new_height) = resolve_dimensions(width, height, new_width, new_height);

    let (scale_w, scale_h, crop_x, crop_y, crop_w, crop_h) = match scale_mode {
        WeaveScaleMode::ScaleToFill => {
            // ScaleToFill: scale up to cover, then center crop
            let x_factor = new_width as f64 / width as f64;
            let y_factor = new_height as f64 / height as f64;
            let scale = x_factor.max(y_factor);
            let sw = ((width as f64 * scale).round() as usize).max(1);
            let sh = ((height as f64 * scale).round() as usize).max(1);
            let cx = ((sw as i64 - new_width as i64) / 2).max(0) as usize;
            let cy = ((sh as i64 - new_height as i64) / 2).max(0) as usize;
            // guard: crop window can't exceed scaled size due to rounding
            let cw = new_width.min(sw);
            let ch = new_height.min(sh);
            (sw, sh, cx, cy, cw, ch)
        }
        WeaveScaleMode::ScaleToFit => {
            // ScaleToFit: scale to fit within bounds, crop excess (will be 0 on one axis)
            let x_factor = new_width as f64 / width as f64;
            let y_factor = new_height as f64 / height as f64;
            let scale = x_factor.min(y_factor);
            let sw = ((width as f64 * scale).round() as usize).max(1);
            let sh = ((height as f64 * scale).round() as usize).max(1);
            let cx = ((sw as i64 - new_width as i64) / 2).max(0) as usize;
            let cy = ((sh as i64 - new_height as i64) / 2).max(0) as usize;
            let cw = sw.min(new_width);
            let ch = sh.min(new_height);
            (sw, sh, cx, cy, cw, ch)
        }
        WeaveScaleMode::JustResize => {
            // JustResize
            (new_width, new_height, 0, 0, new_width, new_height)
        }
    };

    let Ok(mut scaled_store) =
        ImageStoreMut::try_alloc_with_depth(scale_w, scale_h, bit_depth as usize)
    else {
        return ScalingResultGen {
            data: std::ptr::null_mut(),
            width: 0,
            height: 0,
            stride: 0,
            capacity: 0,
            length: 0,
        };
    };

    _ = source_store.scale(&mut scaled_store, options);

    let final_store = if crop_x > 0 || crop_y > 0 || crop_w != scale_w || crop_h != scale_h {
        match scaled_store.crop_with_copy(crop_x, crop_y, crop_w, crop_h) {
            Ok(v) => v,
            Err(_) => {
                return ScalingResultGen {
                    data: std::ptr::null_mut(),
                    width: 0,
                    stride: 0,
                    height: 0,
                    capacity: 0,
                    length: 0,
                }
            }
        }
    } else {
        scaled_store
    };

    let final_width = final_store.width;
    let final_height = final_store.height;

    let stride = final_store.stride();

    let mut final_buffer = match final_store.buffer {
        BufferStore::Borrowed(v) => v.to_vec(),
        BufferStore::Owned(v) => v,
    };

    let data = final_buffer.as_mut_ptr();
    let capacity = final_buffer.capacity();
    let length = final_buffer.len();
    std::mem::forget(final_buffer);

    ScalingResultGen {
        data,
        capacity,
        length,
        stride,
        width: final_width,
        height: final_height,
    }
}

#[no_mangle]
pub unsafe extern "C" fn weave_scale_u8(
    src: *const u8,
    src_stride: u32,
    width: u32,
    height: u32,
    new_width: i32,
    new_height: i32,
    scaling_function: ScalingFunction,
    premultiply_alpha: bool,
    scale_mode: WeaveScaleMode,
) -> ScalingResultU8 {
    let q = pic_scale_scale_generic::<u8, 4>(
        src,
        src_stride as usize,
        width,
        height,
        new_width,
        new_height,
        8,
        scaling_function,
        premultiply_alpha,
        scale_mode,
    );
    ScalingResultU8 {
        data: q.data,
        stride: q.stride,
        width: q.width,
        height: q.height,
        length: q.length,
        capacity: q.capacity,
    }
}

#[no_mangle]
pub unsafe extern "C" fn weave_scale_u16(
    src: *const u16,
    src_stride: usize,
    width: u32,
    height: u32,
    new_width: i32,
    new_height: i32,
    bit_depth: usize,
    scaling_function: ScalingFunction,
    premultiply_alpha: bool,
    scale_mode: WeaveScaleMode,
) -> ScalingResultU16 {
    let q = pic_scale_scale_generic::<u16, 4>(
        src,
        src_stride,
        width,
        height,
        new_width,
        new_height,
        bit_depth as u32,
        scaling_function,
        premultiply_alpha,
        scale_mode,
    );
    ScalingResultU16 {
        data: q.data,
        stride: q.stride,
        width: q.width,
        height: q.height,
        length: q.length,
        capacity: q.capacity,
    }
}
