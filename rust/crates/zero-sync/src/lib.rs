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

        // Safe, zero-copy UnsafeCell array initialization without heap
        let storage: [UnsafeCell<MaybeUninit<T>>; CAPACITY] = unsafe {
            MaybeUninit::uninit().assume_init()
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

    /// Enqueues a contiguous slice in batches (called from Producer / ISR)
    pub fn enqueue_slice(&self, slice: &[T]) -> usize {
        let head = self.head.load(Ordering::Relaxed);
        let tail = self.tail.load(Ordering::Acquire);
        let available = CAPACITY - head.wrapping_sub(tail);
        let to_write = slice.len().min(available);
        if to_write == 0 {
            return 0;
        }

        let idx = head & (CAPACITY - 1);
        let chunk1 = (CAPACITY - idx).min(to_write);
        for (i, item) in slice[..chunk1].iter().enumerate() {
            unsafe {
                let slot = self.storage[idx + i].get();
                (*slot).write(*item);
            }
        }

        let chunk2 = to_write - chunk1;
        if chunk2 > 0 {
            for (i, item) in slice[chunk1..to_write].iter().enumerate() {
                unsafe {
                    let slot = self.storage[i].get();
                    (*slot).write(*item);
                }
            }
        }

        self.head.store(head.wrapping_add(to_write), Ordering::Release);
        to_write
    }

    /// Dequeues items into a destination slice in batches (called from Consumer / Thread)
    pub fn dequeue_slice(&self, dst: &mut [T]) -> usize {
        let tail = self.tail.load(Ordering::Relaxed);
        let head = self.head.load(Ordering::Acquire);
        let count = head.wrapping_sub(tail);
        let to_read = dst.len().min(count);
        if to_read == 0 {
            return 0;
        }

        let idx = tail & (CAPACITY - 1);
        let chunk1 = (CAPACITY - idx).min(to_read);
        for (i, slot_dst) in dst[..chunk1].iter_mut().enumerate() {
            unsafe {
                let slot = self.storage[idx + i].get();
                *slot_dst = (*slot).assume_init();
            }
        }

        let chunk2 = to_read - chunk1;
        if chunk2 > 0 {
            for (i, slot_dst) in dst[chunk1..to_read].iter_mut().enumerate() {
                unsafe {
                    let slot = self.storage[i].get();
                    *slot_dst = (*slot).assume_init();
                }
            }
        }

        self.tail.store(tail.wrapping_add(to_read), Ordering::Release);
        to_read
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
