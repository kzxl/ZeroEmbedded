#![no_std]

//! # zero-sync
//!
//! Lockless ringbuffers, atomic wrappers, and interrupt-safe synchronization.
//! Strictly `#![no_std]` and zero-heap.

use core::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use core::cell::UnsafeCell;
use core::mem::MaybeUninit;
use zero_core::FwStatus;

/* ========================================================================== */
/* 1. SpinLock Flag (Cooperative / Single-Core Embedded)                      */
/* ========================================================================== */

pub struct SpinLockFlag {
    locked: AtomicBool,
}

impl SpinLockFlag {
    pub const fn new() -> Self {
        Self {
            locked: AtomicBool::new(false),
        }
    }

    #[inline(always)]
    pub fn try_lock(&self) -> bool {
        !self.locked.swap(true, Ordering::Acquire)
    }

    #[inline(always)]
    pub fn unlock(&self) {
        self.locked.store(false, Ordering::Release);
    }
}

/* ========================================================================== */
/* 2. SPSC Lock-Free RingBuffer (Single Producer / Single Consumer)           */
/* ========================================================================== */

/// Lock-free Single-Producer Single-Consumer queue ideal for ISR-to-Thread streaming.
/// CAPACITY must be a power of two for efficient bitwise masking.
pub struct SpscQueue<T, const CAPACITY: usize> {
    storage: [UnsafeCell<MaybeUninit<T>>; CAPACITY],
    head: AtomicUsize,
    tail: AtomicUsize,
}

unsafe impl<T: Send, const CAPACITY: usize> Sync for SpscQueue<T, CAPACITY> {}

impl<T: Copy, const CAPACITY: usize> SpscQueue<T, CAPACITY> {
    const CAPACITY_CHECK: () = assert!(CAPACITY > 0 && (CAPACITY & (CAPACITY - 1)) == 0, "Capacity must be power of two");

    pub const fn new() -> Self {
        #[allow(clippy::let_unit_value)]
        let _ = Self::CAPACITY_CHECK;

        // UnsafeCell array initialization without heap
        #[allow(clippy::declare_interior_mutable_const)]
        const UNINIT_CELL: UnsafeCell<MaybeUninit<u8>> = UnsafeCell::new(MaybeUninit::uninit());
        
        // Reinterpreting memory safe for const initializer of MaybeUninit
        let storage = unsafe {
            core::mem::transmute_copy(&[UNINIT_CELL; CAPACITY])
        };

        Self {
            storage,
            head: AtomicUsize::new(0),
            tail: AtomicUsize::new(0),
        }
    }

    #[inline(always)]
    pub fn capacity(&self) -> usize {
        CAPACITY
    }

    /// Tries to push an item (called from Producer / ISR)
    #[inline]
    pub fn try_enqueue(&self, item: T) -> Result<(), FwStatus> {
        let head = self.head.load(Ordering::Relaxed);
        let tail = self.tail.load(Ordering::Acquire);

        if head.wrapping_sub(tail) >= CAPACITY {
            return Err(FwStatus::ErrBufferOverflow);
        }

        let index = head & (CAPACITY - 1);
        unsafe {
            let slot = self.storage[index].get();
            (*slot).write(item);
        }

        self.head.store(head.wrapping_add(1), Ordering::Release);
        Ok(())
    }

    /// Tries to pop an item (called from Consumer / Main Thread)
    #[inline]
    pub fn try_dequeue(&self) -> Option<T> {
        let tail = self.tail.load(Ordering::Relaxed);
        let head = self.head.load(Ordering::Acquire);

        if tail == head {
            return None;
        }

        let index = tail & (CAPACITY - 1);
        let item = unsafe {
            let slot = self.storage[index].get();
            (*slot).assume_init()
        };

        self.tail.store(tail.wrapping_add(1), Ordering::Release);
        Some(item)
    }

    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.head.load(Ordering::Relaxed) == self.tail.load(Ordering::Relaxed)
    }

    #[inline(always)]
    pub fn is_full(&self) -> bool {
        let head = self.head.load(Ordering::Relaxed);
        let tail = self.tail.load(Ordering::Relaxed);
        head.wrapping_sub(tail) >= CAPACITY
    }

    #[inline(always)]
    pub fn len(&self) -> usize {
        let head = self.head.load(Ordering::Relaxed);
        let tail = self.tail.load(Ordering::Relaxed);
        head.wrapping_sub(tail)
    }
}
