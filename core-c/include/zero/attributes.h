#ifndef ZERO_ATTRIBUTES_H
#define ZERO_ATTRIBUTES_H

/**
 * @file attributes.h
 * @brief Compiler attributes and static analyzer annotations for ZeroEmbedded.
 */

/* ========================================================================== */
/* Compiler-Specific Optimization & Inlining Macros                           */
/* ========================================================================== */

#if defined(__clang__) || defined(__GNUC__)
    #define FW_INLINE static inline
    #define FW_ALWAYS_INLINE static inline __attribute__((always_inline))
    #define FW_NOINLINE __attribute__((noinline))
    #define FW_NODISCARD __attribute__((warn_unused_result))
    #define FW_RESTRICT __restrict__
    #define FW_PACKED __attribute__((packed))
    #define FW_ALIGNED(n) __attribute__((aligned(n)))
    #define FW_LIKELY(x) __builtin_expect(!!(x), 1)
    #define FW_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define FW_UNREACHABLE() __builtin_unreachable()
    #define FW_COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")
    #if defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5T__) || defined(__ARM_ARCH_4T__) || (defined(__ARM_ARCH) && __ARM_ARCH < 7)
        /* ARM pre-v7 (ARM946E-S, etc.): CP15 Drain Write Buffer */
        #define FW_MEMORY_BARRIER()   __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory")
    #elif defined(__arm__) || defined(__thumb__)
        #define FW_MEMORY_BARRIER()   __asm__ __volatile__("dmb 0xF" ::: "memory")
    #elif defined(__riscv)
        #define FW_MEMORY_BARRIER()   __asm__ __volatile__("fence rw, rw" ::: "memory")
    #else
        #define FW_MEMORY_BARRIER()   __sync_synchronize()
    #endif
    #define FW_CACHE_ALIGNED(n)       FW_ALIGNED(n)
#elif defined(_MSC_VER)
    #define FW_INLINE static __inline
    #define FW_ALWAYS_INLINE static __forceinline
    #define FW_NOINLINE __declspec(noinline)
    #define FW_NODISCARD _Check_return_
    #define FW_RESTRICT __restrict
    #define FW_PACKED
    #define FW_ALIGNED(n) __declspec(align(n))
    #define FW_LIKELY(x) (x)
    #define FW_UNLIKELY(x) (x)
    #define FW_UNREACHABLE() __assume(0)
    #if defined(_M_X64) || defined(_M_IX86) || defined(_M_ARM) || defined(_M_ARM64)
        #ifdef __cplusplus
        extern "C" {
        #endif
        long _InterlockedOr(long volatile *Target, long Value);
        #ifdef __cplusplus
        }
        #endif
        #pragma intrinsic(_InterlockedOr)
        #define FW_COMPILER_BARRIER() _ReadWriteBarrier()
        #define FW_MEMORY_BARRIER() do { \
            volatile long _fence_dummy = 0; \
            _InterlockedOr(&_fence_dummy, 0); \
        } while(0)
    #else
        #define FW_COMPILER_BARRIER() do {} while(0)
        #define FW_MEMORY_BARRIER()   do {} while(0)
    #endif
    #define FW_CACHE_ALIGNED(n)       FW_ALIGNED(n)
#else
    #define FW_INLINE static inline
    #define FW_ALWAYS_INLINE static inline
    #define FW_NOINLINE
    #define FW_NODISCARD
    #define FW_RESTRICT
    #define FW_PACKED
    #define FW_ALIGNED(n)
    #define FW_CACHE_ALIGNED(n)
    #define FW_LIKELY(x) (x)
    #define FW_UNLIKELY(x) (x)
    #define FW_UNREACHABLE() do {} while(0)
#endif

/* ========================================================================== */
/* Static Analyzer & Ownership Annotations (Phase 4)                          */
/* ========================================================================== */

#if defined(__clang__) && defined(ZERO_STATIC_ANALYSIS)
    #define FW_ANNOTATE(x) __attribute__((annotate(x)))
#else
    #define FW_ANNOTATE(x)
#endif

/** Marks a pointer or handle as an owning resource that must be freed/released */
#define FW_OWNER FW_ANNOTATE("zero::owner")

/** Marks a pointer or handle as a borrowed reference (non-owning) */
#define FW_BORROW FW_ANNOTATE("zero::borrow")

/** Marks a parameter or return value that cannot be null */
#define FW_NONNULL FW_ANNOTATE("zero::nonnull")

/** Marks a pointer that may safely be null */
#define FW_NULLABLE FW_ANNOTATE("zero::nullable")

/** Marks a buffer or memory span as immutable / read-only */
#define FW_READONLY FW_ANNOTATE("zero::readonly")

/** Marks maximum size bounds for a buffer */
#define FW_BOUNDS(n) FW_ANNOTATE("zero::bounds:" #n)

/* ========================================================================== */
/* Execution Context Safety Annotations (Phase 5)                             */
/* ========================================================================== */

/** Marks an interrupt service routine (ISR) context - blocks non-reentrant calls */
#define FW_ISR FW_ANNOTATE("zero::context::isr")

/** Marks memory accessed by DMA - requires lifetime persistence */
#define FW_DMA FW_ANNOTATE("zero::context::dma")

/** Marks system initialization context - dynamic allocation permitted */
#define FW_INIT FW_ANNOTATE("zero::context::init")

/** Standard thread / main loop context */
#ifdef FW_NORMAL
    #undef FW_NORMAL
#endif
#define FW_NORMAL FW_ANNOTATE("zero::context::normal")

#endif /* ZERO_ATTRIBUTES_H */
