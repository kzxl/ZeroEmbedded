#ifndef ZERO_CONFIG_H
#define ZERO_CONFIG_H

/**
 * @file config.h
 * @brief Compile-time configuration flags and architecture detection for ZeroEmbedded.
 */

#include "attributes.h"

/* Enable assertions by default in debug builds */
#ifndef ZERO_ENABLE_ASSERT
    #if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
        #define ZERO_ENABLE_ASSERT 1
    #else
        #define ZERO_ENABLE_ASSERT 0
    #endif
#endif

/* Architecture pointer width detection */
#if defined(__SIZEOF_POINTER__)
    #define ZERO_PTR_SIZE __SIZEOF_POINTER__
#elif defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(__riscv_xlen) && (__riscv_xlen == 64)
    #define ZERO_PTR_SIZE 8
#else
    #define ZERO_PTR_SIZE 4
#endif

/* Alignment requirement */
#ifndef ZERO_DEFAULT_ALIGNMENT
    #define ZERO_DEFAULT_ALIGNMENT sizeof(void*)
#endif

#endif /* ZERO_CONFIG_H */
