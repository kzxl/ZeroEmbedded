//! # Safe ZeroWire Protocol Engine
//!
//! Safe `#![no_std]` binary frame parser and encoder.
//! Guarantees zero buffer overflows and exposes C-ABI compatible FFI functions.

use crate::{FwSpan, FwStringView, FwStatus};

pub const ZEROWIRE_SOF0: u8 = 0xAA;
pub const ZEROWIRE_SOF1: u8 = 0x55;
pub const ZEROWIRE_HEADER_SIZE: usize = 6;
pub const ZEROWIRE_CRC_SIZE: usize = 2;
pub const ZEROWIRE_MAX_PAYLOAD: usize = 256;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct RustZerowireFrame {
    pub seq: u8,
    pub msg_id: u8,
    pub length: u16,
    pub payload: [u8; ZEROWIRE_MAX_PAYLOAD],
}

impl RustZerowireFrame {
    pub const fn empty() -> Self {
        Self {
            seq: 0,
            msg_id: 0,
            length: 0,
            payload: [0u8; ZEROWIRE_MAX_PAYLOAD],
        }
    }
}

/// Computes CRC16-CCITT in pure Rust
pub fn crc16_ccitt(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;
    for &byte in data {
        crc ^= (byte as u16) << 8;
        for _ in 0..8 {
            if (crc & 0x8000) != 0 {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    crc
}

/// Encodes frame into buffer slice safely
pub fn encode_frame(frame: &RustZerowireFrame, dst: &mut [u8]) -> Result<usize, FwStatus> {
    let payload_len = frame.length as usize;
    if payload_len > ZEROWIRE_MAX_PAYLOAD {
        return Err(FwStatus::ErrBufferOverflow);
    }

    let total_size = ZEROWIRE_HEADER_SIZE + payload_len + ZEROWIRE_CRC_SIZE;
    if dst.len() < total_size {
        return Err(FwStatus::ErrBufferOverflow);
    }

    dst[0] = ZEROWIRE_SOF0;
    dst[1] = ZEROWIRE_SOF1;
    dst[2] = frame.seq;
    dst[3] = frame.msg_id;
    dst[4] = (frame.length & 0xFF) as u8;
    dst[5] = ((frame.length >> 8) & 0xFF) as u8;

    if payload_len > 0 {
        dst[6..6 + payload_len].copy_from_slice(&frame.payload[..payload_len]);
    }

    let crc = crc16_ccitt(&dst[..ZEROWIRE_HEADER_SIZE + payload_len]);
    dst[total_size - 2] = (crc & 0xFF) as u8;
    dst[total_size - 1] = ((crc >> 8) & 0xFF) as u8;

    Ok(total_size)
}

/// Decodes frame from buffer slice safely with pattern matching
pub fn decode_frame(src: &[u8]) -> Result<RustZerowireFrame, FwStatus> {
    if src.len() < ZEROWIRE_HEADER_SIZE + ZEROWIRE_CRC_SIZE {
        return Err(FwStatus::ErrNotFound);
    }

    if src[0] != ZEROWIRE_SOF0 || src[1] != ZEROWIRE_SOF1 {
        return Err(FwStatus::ErrCorrupted);
    }

    let payload_len = (src[4] as usize) | ((src[5] as usize) << 8);
    if payload_len > ZEROWIRE_MAX_PAYLOAD {
        return Err(FwStatus::ErrBufferOverflow);
    }

    let total_expected = ZEROWIRE_HEADER_SIZE + payload_len + ZEROWIRE_CRC_SIZE;
    if src.len() < total_expected {
        return Err(FwStatus::ErrNotFound);
    }

    let expected_crc = crc16_ccitt(&src[..ZEROWIRE_HEADER_SIZE + payload_len]);
    let actual_crc = (src[total_expected - 2] as u16) | ((src[total_expected - 1] as u16) << 8);

    if expected_crc != actual_crc {
        return Err(FwStatus::ErrCorrupted);
    }

    let mut frame = RustZerowireFrame::empty();
    frame.seq = src[2];
    frame.msg_id = src[3];
    frame.length = payload_len as u16;

    if payload_len > 0 {
        frame.payload[..payload_len].copy_from_slice(&src[6..6 + payload_len]);
    }

    Ok(frame)
}

/* ========================================================================== */
/* C-ABI Exported Boundary (Rule 4: extern "C")                               */
/* ========================================================================== */

#[no_mangle]
pub unsafe extern "C" fn rust_zerowire_encode(
    frame: *const RustZerowireFrame,
    dst_ptr: *mut u8,
    dst_len: usize,
    out_written: *mut usize,
) -> FwStatus {
    if frame.is_null() || dst_ptr.is_null() || out_written.is_null() {
        return FwStatus::ErrInvalidArgument;
    }

    let slice = core::slice::from_raw_parts_mut(dst_ptr, dst_len);
    match encode_frame(&*frame, slice) {
        Ok(bytes) => {
            *out_written = bytes;
            FwStatus::Ok
        }
        Err(err) => err,
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_zerowire_decode(
    src_ptr: *const u8,
    src_len: usize,
    out_frame: *mut RustZerowireFrame,
) -> FwStatus {
    if src_ptr.is_null() || out_frame.is_null() {
        return FwStatus::ErrInvalidArgument;
    }

    let slice = core::slice::from_raw_parts(src_ptr, src_len);
    match decode_frame(slice) {
        Ok(frame) => {
            *out_frame = frame;
            FwStatus::Ok
        }
        Err(err) => err,
    }
}
