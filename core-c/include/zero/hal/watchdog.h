#ifndef ZERO_HAL_WATCHDOG_H
#define ZERO_HAL_WATCHDOG_H

/**
 * @file watchdog.h
 * @brief Multi-Task Watchdog Supervisor & Liveness Token Tracker
 * Ensures true cooperative multi-task liveness before kicking hardware watchdogs.
 * Eliminates the anti-pattern of refreshing watchdogs blindly inside timer interrupts.
 */

#include "zero/types.h"
#include "zero/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_WDT_MAX_TASKS 32

typedef void (*fw_wdt_kick_fn_t)(void);
typedef void (*fw_wdt_fault_fn_t)(uint32_t starving_mask);

typedef struct {
    uint32_t            registered_mask;  /*!< Bitmask of monitored tasks (1..32) */
    uint32_t            reported_mask;    /*!< Bitmask of tasks that reported in current window */
    uint32_t            timeout_ms;       /*!< Supervision timeout window in milliseconds */
    uint32_t            last_kick_ms;     /*!< Monotonic timestamp of last hardware kick */
    fw_wdt_kick_fn_t    hw_kick_fn;       /*!< Hardware IWDG/WWDG refresh callback */
    fw_wdt_fault_fn_t   fault_fn;         /*!< Optional callback on task starvation */
} fw_wdt_supervisor_t;

/**
 * @brief Initializes the Watchdog Supervisor.
 *
 * @param wdt Pointer to supervisor instance.
 * @param timeout_ms Maximum time window (in ms) allowed between full task check-ins.
 * @param hw_kick_fn Function pointer to kick actual hardware watchdog (can be NULL for tests).
 * @param fault_fn Optional callback invoked if one or more tasks fail to report (can be NULL).
 * @return FW_OK on success, or FW_ERR_INVALID_PARAM if wdt is NULL or timeout_ms is 0.
 */
fw_status_t fw_wdt_init(fw_wdt_supervisor_t *wdt,
                        uint32_t timeout_ms,
                        fw_wdt_kick_fn_t hw_kick_fn,
                        fw_wdt_fault_fn_t fault_fn);

/**
 * @brief Registers a task ID (0..31) for watchdog supervision.
 */
fw_status_t fw_wdt_register_task(fw_wdt_supervisor_t *wdt, uint8_t task_id);

/**
 * @brief Unregisters a task ID from supervision.
 */
fw_status_t fw_wdt_unregister_task(fw_wdt_supervisor_t *wdt, uint8_t task_id);

/**
 * @brief Reports a task's heartbeat / liveness token.
 * Can be safely called from ISR or superloop tasklets.
 */
void fw_wdt_heartbeat(fw_wdt_supervisor_t *wdt, uint8_t task_id);

/**
 * @brief Evaluates task liveness and refreshes hardware watchdog if all tasks reported.
 * Must be called periodically in the main superloop or supervision task.
 *
 * @param wdt Pointer to supervisor instance.
 * @return FW_TRUE if watchdog was successfully fed or still within deadline;
 *         FW_FALSE if a deadline expired with starving tasks.
 */
fw_bool_t fw_wdt_service(fw_wdt_supervisor_t *wdt);

/**
 * @brief Returns bitmask of tasks that have NOT reported in the current window.
 */
uint32_t fw_wdt_get_starving_tasks(const fw_wdt_supervisor_t *wdt);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_WATCHDOG_H */
