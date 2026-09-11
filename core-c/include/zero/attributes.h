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
#else
    #define FW_INLINE static inline
    #define FW_ALWAYS_INLINE static inline
    #define FW_NOINLINE
    #define FW_NODISCARD
    #define FW_RESTRICT
    #define FW_PACKED
    #define FW_ALIGNED(n)
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
