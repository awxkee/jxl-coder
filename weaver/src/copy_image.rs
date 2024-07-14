#[cfg(all(target_arch = "aarch64", target_feature = "neon"))]
use std::arch::aarch64::*;
use num_traits::FromPrimitive;

pub fn copy_image<T: FromPrimitive + Copy>(
    source: &[T],
    src_stride: u32,
    dst: &mut [T],
    dst_stride: u32,
    width: u32,
    height: u32,
    components_count: u32,
) {
    for y in 0..height as usize {
        let dst_ptr = unsafe { (dst.as_mut_ptr() as *mut u8).add(y * dst_stride as usize) };
        let src_ptr = unsafe { (source.as_ptr() as *const u8).add(y * src_stride as usize) };
        let row_length = width as usize * components_count as usize * std::mem::size_of::<T>();
        let mut cx = 0usize;
        #[cfg(all(target_arch = "aarch64", target_feature = "neon"))]
        while cx + 64 < row_length {
            let row = unsafe { vld1q_u8_x4(src_ptr.add(cx)) };
            unsafe {
                vst1q_u8_x4(dst_ptr.add(cx), row);
            }
            cx += 64;
        }
        #[cfg(all(target_arch = "aarch64", target_feature = "neon"))]
        while cx + 32 < row_length {
            let row = unsafe { vld1q_u8_x2(src_ptr.add(cx)) };
            unsafe {
                vst1q_u8_x2(dst_ptr.add(cx), row);
            }
            cx += 32;
        }
        #[cfg(all(target_arch = "aarch64", target_feature = "neon"))]
        while cx + 16 < row_length {
            let row = unsafe { vld1q_u8(src_ptr.add(cx)) };
            unsafe {
                vst1q_u8(dst_ptr.add(cx), row);
            }
            cx += 16;
        }
        #[cfg(all(target_arch = "aarch64", target_feature = "neon"))]
        while cx + 8 < row_length {
            let row = unsafe { vld1_u8(src_ptr.add(cx)) };
            unsafe {
                vst1_u8(dst_ptr.add(cx), row);
            }
            cx += 8;
        }
        while cx < row_length {
            unsafe {
                dst_ptr
                    .add(cx)
                    .write_unaligned(src_ptr.add(cx).read_unaligned());
            }
            cx += 1;
        }
    }
}
