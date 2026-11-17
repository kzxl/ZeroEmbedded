/**
 * @file watchdog.c
 * @brief Multi-Task Watchdog Supervisor implementation
 */

#include "zero/hal/watchdog.h"
#include "zero/hal/timer.h"
#include "zero/attributes.h"

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

    FW_CRITICAL_ENTER();
    wdt->registered_mask |= (1U << task_id);
    /* Initial grace heartbeat upon registration */
    wdt->reported_mask   |= (1U << task_id);
    FW_CRITICAL_EXIT();

    return FW_OK;
}

fw_status_t fw_wdt_unregister_task(fw_wdt_supervisor_t *wdt, uint8_t task_id) {
    if (wdt == FW_NULL || task_id >= FW_WDT_MAX_TASKS) {
        return FW_ERR_INVALID_ARG;
    }

    FW_CRITICAL_ENTER();
    wdt->registered_mask &= ~(1U << task_id);
    wdt->reported_mask   &= ~(1U << task_id);
    FW_CRITICAL_EXIT();

    return FW_OK;
}

void fw_wdt_heartbeat(fw_wdt_supervisor_t *wdt, uint8_t task_id) {
    if (wdt != FW_NULL && task_id < FW_WDT_MAX_TASKS) {
        FW_CRITICAL_ENTER();
        wdt->reported_mask |= (1U << task_id);
        FW_CRITICAL_EXIT();
    }
}

uint32_t fw_wdt_get_starving_tasks(const fw_wdt_supervisor_t *wdt) {
    if (wdt == FW_NULL) {
        return 0;
    }
    FW_CRITICAL_ENTER();
    uint32_t starving = wdt->registered_mask & (~wdt->reported_mask);
    FW_CRITICAL_EXIT();
    return starving;
}

fw_bool_t fw_wdt_service(fw_wdt_supervisor_t *wdt) {
    if (wdt == FW_NULL) {
        return FW_FALSE;
    }

    uint32_t now = fw_timer_get_millis();

    FW_CRITICAL_ENTER();
    uint32_t starving = wdt->registered_mask & (~wdt->reported_mask);
    if (starving == 0) {
        /* Clear window before releasing critical section so concurrent ISR heartbeats are kept */
        wdt->reported_mask = 0;
        FW_CRITICAL_EXIT();

        if (wdt->hw_kick_fn != FW_NULL) {
            wdt->hw_kick_fn();
        }
        wdt->last_kick_ms = now;
        return FW_TRUE;
    }
    FW_CRITICAL_EXIT();

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
