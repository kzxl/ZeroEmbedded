#![no_std]

//! # zero-sync
//!
//! Lockless ringbuffers, atomic wrappers, and interrupt-safe synchronization.

use core::sync::atomic::{AtomicBool, Ordering};

/// Lightweight spinlock flag for single-core bare-metal / cooperative systems
pub struct SpinLockFlag {
    locked: AtomicBool,
}

impl SpinLockFlag {
    pub const fn new() -> Self {
        Self {
            locked: AtomicBool::new(false),
        }
    }

    pub fn try_lock(&self) -> bool {
        !self.locked.swap(true, Ordering::Acquire)
    }

    pub fn unlock(&self) {
        self.locked.store(false, Ordering::Release);
    }
}
