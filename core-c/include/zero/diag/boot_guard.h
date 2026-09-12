#ifndef ZERO_DIAG_BOOT_GUARD_H
#define ZERO_DIAG_BOOT_GUARD_H

/**
 * @file boot_guard.h
 * @brief Boot Loop Guard & Automatic Safe-Mode Recovery Engine.
 * Inspired by Magic Lantern's LOADING.LCK and boot crash mitigation patterns.
 */

#include "../types.h"
#include "../result.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_BOOT_GUARD_DEFAULT_MAX_CRASHES 3

typedef struct {
    uint32_t   boot_attempts;
    uint32_t   consecutive_crashes;
    uint32_t   max_allowed_crashes;
    fw_bool_t  safe_mode_active;
} fw_boot_guard_t;

/**
 * @brief Initializes the boot guard and checks for crash loop conditions.
 *
 * @param guard Pointer to boot guard state (often mapped to persistent RTC/NVS registers).
 * @param max_crashes Maximum consecutive crashes before engaging safe mode (0 for default).
 */
fw_status_t fw_boot_guard_init(fw_boot_guard_t *guard, uint32_t max_crashes);

/**
 * @brief Marks the current boot sequence as fully successful and stable.
 * Resets the consecutive crash counter.
 */
fw_status_t fw_boot_guard_mark_success(fw_boot_guard_t *guard);

/**
 * @brief Manually forces or clears safe mode state (e.g. from user keypress at boot).
 */
fw_status_t fw_boot_guard_set_safe_mode(fw_boot_guard_t *guard, fw_bool_t enable);

/**
 * @brief Returns true if system is currently in safe mode.
 */
fw_bool_t fw_boot_guard_is_safe_mode(const fw_boot_guard_t *guard);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_DIAG_BOOT_GUARD_H */
