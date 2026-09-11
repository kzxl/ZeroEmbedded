#ifndef ZERO_ASSERT_H
#define ZERO_ASSERT_H

/**
 * @file assert.h
 * @brief Zero-cost static and runtime assertions for ZeroEmbedded.
 */

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compile-time static assert */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
    #define FW_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#elif defined(__cplusplus) && (__cplusplus >= 201103L)
    #define FW_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
    #define FW_STATIC_ASSERT_JOIN(a, b) a##b
    #define FW_STATIC_ASSERT_XJOIN(a, b) FW_STATIC_ASSERT_JOIN(a, b)
    #define FW_STATIC_ASSERT(cond, msg) \
        typedef char FW_STATIC_ASSERT_XJOIN(fw_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/** Assert failure handler signature */
typedef void (*fw_assert_handler_t)(const char *file, int line, const char *expr);

/** Set custom assert failure hook */
void fw_set_assert_handler(fw_assert_handler_t handler);

/** Internal panic routine */
FW_NOINLINE void fw_assert_failed(const char *file, int line, const char *expr);

#if ZERO_ENABLE_ASSERT
    #define FW_ASSERT(expr) \
        do { \
            if (FW_UNLIKELY(!(expr))) { \
                fw_assert_failed(__FILE__, __LINE__, #expr); \
            } \
        } while(0)
#else
    #define FW_ASSERT(expr) do { (void)sizeof(expr); } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZERO_ASSERT_H */
