#ifndef ZERO_HAL_CACHE_H
#define ZERO_HAL_CACHE_H

/**
 * @file cache.h
 * @brief Hardware cache coherency primitives (Data/Instruction Cache management)
 * Inspired by low-level DMA & CP15 cache manipulation in camera and embedded systems.
 */

#include "../types.h"
#include "../result.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FW_CACHE_LINE_SIZE
    #if defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_8M_MAIN__)
        #define FW_CACHE_LINE_SIZE 32
    #elif defined(__aarch64__)
        #define FW_CACHE_LINE_SIZE 64
    #else
        #define FW_CACHE_LINE_SIZE 32
    #endif
#endif

/**
 * @brief Cleans (flushes/writes back) Data Cache to RAM for the specified memory range.
 * Must be called BEFORE starting a peripheral DMA transfer from RAM to peripheral.
 */
void fw_cache_clean(const void *addr, fw_size_t size);

/**
 * @brief Invalidates Data Cache lines for the specified memory range.
 * Must be called AFTER a peripheral DMA transfer into RAM completes, before CPU reads the buffer.
 */
void fw_cache_invalidate(void *addr, fw_size_t size);

/**
 * @brief Cleans and invalidates the entire Data Cache.
 */
void fw_cache_flush_all(void);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_CACHE_H */
