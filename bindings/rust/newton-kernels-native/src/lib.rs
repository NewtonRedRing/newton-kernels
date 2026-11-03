//! In-process bindings to an explicitly built NewtonKernels shared C ABI.
//! No background service, JIT, mandatory thread pool or external dependencies.
//! Linker must find newton_c; set RUSTFLAGS=-Lnative=/path/to/library.

use std::ffi::c_int;

#[link(name = "newton_c")]
unsafe extern "C" {
    fn newton_count_byte(data: *const u8, size: usize, needle: u8, count: *mut usize) -> c_int;
    fn newton_sum_gt_u64(
        values: *const u64, size: usize, threshold: u64,
        count: *mut u64, sum: *mut u64,
    ) -> c_int;
}

pub fn count_byte(data: &[u8], needle: u8) -> Result<usize, i32> {
    let mut count = 0_usize;
    let status = unsafe {
        newton_count_byte(data.as_ptr(), data.len(), needle, &mut count)
    };
    if status == 0 { Ok(count) } else { Err(status) }
}

pub fn sum_gt(values: &[u64], threshold: u64) -> Result<(u64, u64), i32> {
    let mut count = 0_u64;
    let mut sum = 0_u64;
    let status = unsafe {
        newton_sum_gt_u64(values.as_ptr(), values.len(), threshold, &mut count, &mut sum)
    };
    if status == 0 { Ok((count, sum)) } else { Err(status) }
}
