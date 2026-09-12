#include "zero/hal/cache.h"

static volatile uint32_t s_cache_clean_ops = 0;
static volatile uint32_t s_cache_invalidate_ops = 0;

void fw_cache_clean(const void *addr, fw_size_t size) {
    if (addr == FW_NULL || size == 0) {
        return;
    }

    FW_MEMORY_BARRIER();

#if defined(__ARM_ARCH_5TE__) || (defined(__ARM_ARCH) && __ARM_ARCH < 7)
    /* ARM946E-S CP15: Clean D-Cache line by address */
    uintptr_t start = (uintptr_t)addr & ~(FW_CACHE_LINE_SIZE - 1);
    uintptr_t end = (uintptr_t)addr + size;
    for (uintptr_t line = start; line < end; line += FW_CACHE_LINE_SIZE) {
        __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 1" :: "r"(line) : "memory");
    }
    /* Drain write buffer */
    __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_MAIN__)
    /* Cortex-M7 / main profile cache operations if available */
    FW_COMPILER_BARRIER();
#else
    /* Portable / Host fallback */
    FW_COMPILER_BARRIER();
#endif

    s_cache_clean_ops++;
    FW_MEMORY_BARRIER();
}

void fw_cache_invalidate(void *addr, fw_size_t size) {
    if (addr == FW_NULL || size == 0) {
        return;
    }

    FW_MEMORY_BARRIER();

#if defined(__ARM_ARCH_5TE__) || (defined(__ARM_ARCH) && __ARM_ARCH < 7)
    /* ARM946E-S CP15: Invalidate D-Cache line by address */
    uintptr_t start = (uintptr_t)addr & ~(FW_CACHE_LINE_SIZE - 1);
    uintptr_t end = (uintptr_t)addr + size;
    for (uintptr_t line = start; line < end; line += FW_CACHE_LINE_SIZE) {
        __asm__ __volatile__("mcr p15, 0, %0, c7, c6, 1" :: "r"(line) : "memory");
    }
#else
    FW_COMPILER_BARRIER();
#endif

    s_cache_invalidate_ops++;
    FW_MEMORY_BARRIER();
}

void fw_cache_flush_all(void) {
    FW_MEMORY_BARRIER();

#if defined(__ARM_ARCH_5TE__) || (defined(__ARM_ARCH) && __ARM_ARCH < 7)
    /* Flush entire D-Cache and drain write buffer */
    __asm__ __volatile__("mcr p15, 0, %0, c7, c6, 0" :: "r"(0) : "memory");
    __asm__ __volatile__("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");
#endif

    s_cache_clean_ops++;
    s_cache_invalidate_ops++;
    FW_MEMORY_BARRIER();
}
