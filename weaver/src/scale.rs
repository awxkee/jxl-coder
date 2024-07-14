use std::slice;
use half::f16;
use pic_scale::{ImageSize, ImageStore, Scaler, Scaling, ThreadingPolicy};
use crate::copy_image::copy_image;
use crate::scaling_function::ScalingFunction;

#[no_mangle]
pub extern "C-unwind" fn weave_scale_f16(
    src: *const u16,
    src_stride: u32,
    width: u32,
    height: u32,
    dst: *const u16,
    dst_stride: u32,
    new_width: u32,
    new_height: u32,
    scaling_function: ScalingFunction,
) {
    let src_slice = unsafe { slice::from_raw_parts(src as *const u8, src_stride as usize * height as usize) };
    let mut source_store_slice = vec![0; height as usize * src_stride as usize];

    let source_store_slice_stride = width * 4 * std::mem::size_of::<f16>() as u32;

    copy_image(src_slice, src_stride, &mut source_store_slice, source_store_slice_stride, width, height, 8);

    let source_store = ImageStore::<f16, 4>::new(unsafe { std::mem::transmute(source_store_slice) }, width as usize, height as usize);

    let mut scaler = Scaler::new(scaling_function.to_resampling_function());
    scaler.set_threading_policy(ThreadingPolicy::Adaptive);
    let dst_slice =
        unsafe { slice::from_raw_parts_mut(dst as *mut u8, dst_stride as usize * new_height as usize) };

    let new_store_stride = new_width * 4 * std::mem::size_of::<f16>() as u32;

    let new_store = scaler.resize_rgba_f16(ImageSize::new(new_width as usize, new_height as usize), source_store);

    let scaled_store = unsafe { std::mem::transmute(new_store.as_bytes()) };

    copy_image(scaled_store, new_store_stride, dst_slice, dst_stride, new_width, new_height, 8);
}

#[no_mangle]
pub extern "C-unwind" fn weave_scale_u8(
    src: *const u8,
    src_stride: u32,
    width: u32,
    height: u32,
    dst: *const u8,
    dst_stride: u32,
    new_width: u32,
    new_height: u32,
    scaling_function: ScalingFunction,
) {
    let src_slice = unsafe { slice::from_raw_parts(src, src_stride as usize * height as usize) };
    let mut source_store_slice = vec![0; height as usize * src_stride as usize];
    let source_store_slice_stride = width * 4;

    copy_image(src_slice, src_stride, &mut source_store_slice, source_store_slice_stride, width, height, 4);

    let source_store = ImageStore::<u8, 4>::new(source_store_slice, width as usize, height as usize);

    let mut scaler = Scaler::new(scaling_function.to_resampling_function());
    scaler.set_threading_policy(ThreadingPolicy::Adaptive);
    let dst_slice =
        unsafe { slice::from_raw_parts_mut(dst as *mut u8, dst_stride as usize * new_height as usize) };

    let new_store_stride = new_width * 4;

    let new_store = scaler.resize_rgba(ImageSize::new(new_width as usize, new_height as usize), source_store, true);

    copy_image(new_store.as_bytes(), new_store_stride, dst_slice, dst_stride, new_width, new_height, 4);
}
