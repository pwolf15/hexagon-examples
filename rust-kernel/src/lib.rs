#![no_std]
#![feature(stdarch_hexagon)]
#![feature(hexagon_target_feature)]

use core::arch::hexagon::v128::*;

const VEC_BYTES: usize = 128;

/// u8 dot product: vrmpyacc vertical + vror log-tree horizontal.
/// Caller guarantees: a, b 128-aligned, len % 128 == 0.
/// # Safety: raw pointers, alignment per contract above.
#[no_mangle]
#[target_feature(enable = "hvx-length128b")]
pub unsafe extern "C" fn rust_dot_u8(a: *const u8, b: *const u8, len: i32) -> u32 {
    let nvec = len as usize / VEC_BYTES;
    let va = a as *const HvxVector;
    let vb = b as *const HvxVector;

    let mut acc = Q6_V_vsplat_R(0);
    for i in 0..nvec {
        acc = Q6_Vuw_vrmpyacc_VuwVubVub(acc, *va.add(i), *vb.add(i));
    }

    // log-tree: rotate-and-add, 5 rounds
    let mut s = (VEC_BYTES / 2) as i32;
    while s >= 4 {
        acc = Q6_Vw_vadd_VwVw(acc, Q6_V_vror_VR(acc, s));
        s >>= 1;
    }

    // lane 0 out via aligned store
    #[repr(C, align(128))]
    struct Lanes([u32; 32]);
    let mut lanes = Lanes([0; 32]);
    core::ptr::write(lanes.0.as_mut_ptr() as *mut HvxVector, acc);
    lanes.0[0]
}

#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! {
    loop {}
}