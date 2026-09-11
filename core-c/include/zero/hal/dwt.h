#ifndef ZERO_HAL_DWT_H
#define ZERO_HAL_DWT_H

/**
 * @file dwt.h
 * @brief Cycle-accurate CPU performance profiling & WCET benchmarking.
 *
 * Provides single-cycle accuracy via ARM Cortex-M DWT (CYCCNT) or
 * host x86/x64 RDTSC hardware timestamp counter.
 */

#include "../types.h"
#include "../attributes.h"

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enables the hardware CPU cycle counter.
 * On ARM Cortex-M: Configures CoreDebug->DEMCR and DWT->CTRL.
 */
FW_INLINE void fw_dwt_init(void) {
#if defined(__arm__) || defined(__thumb__)
    /* CoreDebug_DEMCR_TRCENA_Msk = 0x01000000 */
    volatile uint32_t *demcr = (volatile uint32_t*)0xE000EDFC;
    volatile uint32_t *dwt_ctrl = (volatile uint32_t*)0xE0001000;
    volatile uint32_t *dwt_cyccnt = (volatile uint32_t*)0xE0001004;

    *demcr |= 0x01000000;
    *dwt_cyccnt = 0;
    *dwt_ctrl |= 1; /* DWT_CTRL_CYCCNTENA_Msk */
#endif
}

/**
 * @brief Reads the current CPU hardware cycle counter.
 */
FW_ALWAYS_INLINE uint32_t fw_dwt_get_cycles(void) {
#if defined(__arm__) || defined(__thumb__)
    volatile uint32_t *dwt_cyccnt = (volatile uint32_t*)0xE0001004;
    return *dwt_cyccnt;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    return (uint32_t)__rdtsc();
#elif defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return lo;
#else
    return 0;
#endif
}

/**
 * @brief Calculates elapsed CPU cycles between two readings, handling 32-bit overflow.
 */
FW_ALWAYS_INLINE uint32_t fw_dwt_elapsed_cycles(uint32_t start_cycles, uint32_t end_cycles) {
    return (end_cycles >= start_cycles) ?
           (end_cycles - start_cycles) :
           ((0xFFFFFFFFU - start_cycles) + end_cycles + 1U);
}

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_DWT_H */
