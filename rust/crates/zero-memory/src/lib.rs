#![no_std]

//! # zero-memory
//!
//! Safe memory pools, arena allocators, and buffer ownership primitives.
//! Guaranteed zero dynamic heap (`alloc`) dependency.

use zero_core::FwSpan;

/// Fixed-capacity static buffer
pub struct StaticBuffer<const CAPACITY: usize> {
    storage: [u8; CAPACITY],
    len: usize,
}

impl<const CAPACITY: usize> StaticBuffer<CAPACITY> {
    pub const fn new() -> Self {
        Self {
            storage: [0u8; CAPACITY],
            len: 0,
        }
    }

    pub const fn capacity(&self) -> usize {
        CAPACITY
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    pub fn clear(&mut self) {
        self.len = 0;
    }

    pub fn as_span_mut(&mut self) -> FwSpan {
        FwSpan::from_slice_mut(&mut self.storage[..self.len])
    }
}
