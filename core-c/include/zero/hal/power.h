#ifndef ZERO_HAL_POWER_H
#define ZERO_HAL_POWER_H

/**
 * @file power.h
 * @brief Ultra-low power management, WFI sleep scheduling, and CPU load metrics.
 */

#include "../types.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FW_POWER_MODE_ACTIVE     = 0,
    FW_POWER_MODE_SLEEP      = 1, /* Wait For Interrupt (__WFI) */
    FW_POWER_MODE_DEEP_SLEEP = 2  /* Stop / Standby Mode */
} fw_power_mode_t;

typedef struct {
    uint64_t total_active_us;
    uint64_t total_sleep_us;
    uint32_t sleep_count;
    uint16_t cpu_load_permille; /* 0..1000 (e.g. 25 = 2.5% CPU load) */
} fw_pm_metrics_t;

/** Initializes power management subsystem */
void fw_pm_init(void);

/**
 * @brief Enters low-power sleep mode until an interrupt occurs or timeout expires.
 * On ARM Cortex-M: Executes __WFI() with tickless accounting.
 * On Host / Sim : Executes high-resolution micro-sleep.
 *
 * @param max_sleep_ms Maximum duration to sleep in milliseconds (0 = indefinite until interrupt).
 */
void fw_pm_enter_sleep(uint32_t max_sleep_ms);

/** Retrieves active CPU power and duty-cycle metrics */
void fw_pm_get_metrics(fw_pm_metrics_t *out_metrics);

/** Resets measurement period for CPU duty-cycle calculation */
void fw_pm_reset_metrics(void);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_POWER_H */
