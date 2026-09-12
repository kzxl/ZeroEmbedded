#include "zero/diag/boot_guard.h"

fw_status_t fw_boot_guard_init(fw_boot_guard_t *guard, uint32_t max_crashes) {
    if (guard == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    if (max_crashes == 0) {
        max_crashes = FW_BOOT_GUARD_DEFAULT_MAX_CRASHES;
    }
    guard->max_allowed_crashes = max_crashes;

    guard->boot_attempts++;
    guard->consecutive_crashes++;

    if (guard->consecutive_crashes >= guard->max_allowed_crashes) {
        guard->safe_mode_active = FW_TRUE;
    }

    return FW_OK;
}

fw_status_t fw_boot_guard_mark_success(fw_boot_guard_t *guard) {
    if (guard == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    guard->consecutive_crashes = 0;
    guard->safe_mode_active = FW_FALSE;
    return FW_OK;
}

fw_status_t fw_boot_guard_set_safe_mode(fw_boot_guard_t *guard, fw_bool_t enable) {
    if (guard == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    guard->safe_mode_active = enable;
    return FW_OK;
}

fw_bool_t fw_boot_guard_is_safe_mode(const fw_boot_guard_t *guard) {
    if (guard == FW_NULL) {
        return FW_FALSE;
    }
    return guard->safe_mode_active;
}
