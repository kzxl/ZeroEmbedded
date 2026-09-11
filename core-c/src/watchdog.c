/**
 * @file watchdog.c
 * @brief Multi-Task Watchdog Supervisor implementation
 */

#include "zero/hal/watchdog.h"
#include "zero/hal/timer.h"

fw_status_t fw_wdt_init(fw_wdt_supervisor_t *wdt,
                        uint32_t timeout_ms,
                        fw_wdt_kick_fn_t hw_kick_fn,
                        fw_wdt_fault_fn_t fault_fn) {
    if (wdt == FW_NULL || timeout_ms == 0) {
        return FW_ERR_INVALID_ARG;
    }

    wdt->registered_mask = 0;
    wdt->reported_mask   = 0;
    wdt->timeout_ms       = timeout_ms;
    wdt->last_kick_ms     = fw_timer_get_millis();
    wdt->hw_kick_fn      = hw_kick_fn;
    wdt->fault_fn        = fault_fn;

    return FW_OK;
}

fw_status_t fw_wdt_register_task(fw_wdt_supervisor_t *wdt, uint8_t task_id) {
    if (wdt == FW_NULL || task_id >= FW_WDT_MAX_TASKS) {
        return FW_ERR_INVALID_ARG;
    }

    wdt->registered_mask |= (1U << task_id);
    /* Initial grace heartbeat upon registration */
    wdt->reported_mask   |= (1U << task_id);

    return FW_OK;
}

fw_status_t fw_wdt_unregister_task(fw_wdt_supervisor_t *wdt, uint8_t task_id) {
    if (wdt == FW_NULL || task_id >= FW_WDT_MAX_TASKS) {
        return FW_ERR_INVALID_ARG;
    }

    wdt->registered_mask &= ~(1U << task_id);
    wdt->reported_mask   &= ~(1U << task_id);

    return FW_OK;
}

void fw_wdt_heartbeat(fw_wdt_supervisor_t *wdt, uint8_t task_id) {
    if (wdt != FW_NULL && task_id < FW_WDT_MAX_TASKS) {
        wdt->reported_mask |= (1U << task_id);
    }
}

uint32_t fw_wdt_get_starving_tasks(const fw_wdt_supervisor_t *wdt) {
    if (wdt == FW_NULL) {
        return 0;
    }
    return wdt->registered_mask & (~wdt->reported_mask);
}

fw_bool_t fw_wdt_service(fw_wdt_supervisor_t *wdt) {
    if (wdt == FW_NULL) {
        return FW_FALSE;
    }

    uint32_t starving = fw_wdt_get_starving_tasks(wdt);
    uint32_t now = fw_timer_get_millis();

    if (starving == 0) {
        /* All registered tasks checked in -> Refresh hardware watchdog */
        if (wdt->hw_kick_fn != FW_NULL) {
            wdt->hw_kick_fn();
        }
        wdt->reported_mask = 0;
        wdt->last_kick_ms  = now;
        return FW_TRUE;
    }

    /* Check if timeout expired while some tasks are starving */
    if ((now - wdt->last_kick_ms) >= wdt->timeout_ms) {
        if (wdt->fault_fn != FW_NULL) {
            wdt->fault_fn(starving);
        }
        return FW_FALSE;
    }

    /* Still waiting within current window */
    return FW_TRUE;
}
