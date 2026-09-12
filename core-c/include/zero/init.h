#ifndef ZERO_INIT_H
#define ZERO_INIT_H

/**
 * @file init.h
 * @brief Linker Table Auto-Registration and Modular Initialization Engine.
 * Inspired by Linux __initcall and Magic Lantern INIT_FUNC pattern.
 */

#include "types.h"
#include "result.h"
#include "attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FW_INIT_LEVEL_EARLY  = 0, /**< Clocks, Cache, MPU, Power */
    FW_INIT_LEVEL_CORE   = 1, /**< Memory Hub, SPSC, Timers */
    FW_INIT_LEVEL_DRIVER = 2, /**< GPIO, UART, DMA, Flash/NVS */
    FW_INIT_LEVEL_APP    = 3, /**< Tasks, Protocols, Applications */
    FW_INIT_LEVEL_COUNT  = 4
} fw_init_level_t;

typedef fw_status_t (*fw_init_fn_t)(void);

typedef struct {
    const char     *name;
    fw_init_fn_t    init_fn;
    uint8_t         level;
    uint8_t         priority;
} fw_init_entry_t;

#define FW_INIT_MAX_ENTRIES 32

/**
 * @brief Registers an initialization function into the auto-init engine.
 */
fw_status_t fw_init_register(const char *name, fw_init_fn_t fn, uint8_t level, uint8_t priority);

/**
 * @brief Executes all registered initialization functions for a specific level in priority order.
 */
fw_status_t fw_init_level(fw_init_level_t level);

/**
 * @brief Executes all registered initialization functions from LEVEL_EARLY to LEVEL_APP.
 */
fw_status_t fw_init_all(void);

/**
 * @brief Resets initialization registry state (useful for test suites).
 */
void fw_init_reset(void);

/**
 * @brief Returns total number of registered init entries.
 */
fw_size_t fw_init_count(void);

#if defined(__GNUC__) || defined(__clang__)
    #define FW_INIT_EXPORT(fn, lvl, prio) \
        static fw_status_t __fw_call_##fn(void) { return fn(); } \
        __attribute__((constructor)) static void __fw_auto_reg_##fn(void) { \
            fw_init_register(#fn, __fw_call_##fn, (uint8_t)(lvl), (uint8_t)(prio)); \
        }
#else
    #define FW_INIT_EXPORT(fn, lvl, prio) \
        /* Call fw_init_register(#fn, fn, lvl, prio) during system startup */
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZERO_INIT_H */
