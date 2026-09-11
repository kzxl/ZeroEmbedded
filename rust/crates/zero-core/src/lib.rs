#![no_std]

//! # zero-core
//!
//! Core primitives and C-ABI compatibility bridge for ZeroEmbedded.
//! Designed strictly for `#![no_std]` bare-metal microcontroller targets.

use core::ffi::c_void;

pub mod protocol;
pub use protocol::{RustZerowireFrame, encode_frame, decode_frame};

/// C-ABI compatible status code matching `fw_status_t`
#[repr(i32)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum FwStatus {
    Ok = 0,
    ErrGeneric = -1,
    ErrInvalidArgument = -2,
    ErrOutOfMemory = -3,
    ErrBufferOverflow = -4,
    ErrTimeout = -5,
    ErrBusy = -6,
    ErrNotFound = -7,
    ErrNotSupported = -8,
    ErrIo = -9,
    ErrCorrupted = -10,
    ErrIsrViolation = -11,
    ErrDmaUnaligned = -12,
}

/// C-ABI compatible mutable memory span matching `fw_span_t`
#[repr(C)]
#[derive(Copy, Clone)]
pub struct FwSpan {
    pub data: *mut c_void,
    pub length: usize,
}

impl FwSpan {
    pub const fn empty() -> Self {
        Self {
            data: core::ptr::null_mut(),
            length: 0,
        }
    }

    /// Converts safe Rust byte slice to C-ABI span
    pub fn from_slice_mut(slice: &mut [u8]) -> Self {
        Self {
            data: slice.as_mut_ptr() as *mut c_void,
            length: slice.len(),
        }
    }

    /// Converts C-ABI span into safe Rust byte slice
    ///
    /// # Safety
    /// Caller must guarantee `data` is valid for reads and writes up to `length` bytes.
    pub unsafe fn as_slice_mut<'a>(&self) -> &'a mut [u8] {
        if self.data.is_null() || self.length == 0 {
            &mut []
        } else {
            core::slice::from_raw_parts_mut(self.data as *mut u8, self.length)
        }
    }
}

/// C-ABI compatible string view matching `fw_string_view_t`
#[repr(C)]
#[derive(Copy, Clone)]
pub struct FwStringView {
    pub data: *const u8,
    pub length: usize,
}

impl FwStringView {
    pub const fn from_str(s: &str) -> Self {
        Self {
            data: s.as_ptr(),
            length: s.len(),
        }
    }

    /// Converts to safe Rust str
    ///
    /// # Safety
    /// Caller must guarantee `data` points to valid UTF-8 memory for `length` bytes.
    pub unsafe fn as_str<'a>(&self) -> Result<&'a str, core::str::Utf8Error> {
        if self.data.is_null() || self.length == 0 {
            Ok("")
        } else {
            let bytes = core::slice::from_raw_parts(self.data, self.length);
            core::str::from_utf8(bytes)
        }
    }
}
