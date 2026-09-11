#include "zero/rtos/rtos.h"
#include "zero/hal/timer.h"

/* Bare-metal cooperative fallback implementation */

fw_status_t fw_task_create(const fw_task_config_t *cfg, fw_task_handle_t *out_handle) {
    if (cfg == FW_NULL) return FW_ERR_INVALID_ARG;
    if (out_handle != FW_NULL) *out_handle = (void*)cfg->entry_point;
    return FW_OK;
}

void fw_task_delay_ms(uint32_t ms) {
    fw_delay_millis(ms);
}

void fw_task_yield(void) {
    /* Cooperative yield (no-op in basic baremetal superloop) */
}

fw_status_t fw_mutex_init(fw_mutex_handle_t *out_mutex) {
    if (out_mutex != FW_NULL) *out_mutex = (void*)1;
    return FW_OK;
}

fw_status_t fw_mutex_lock(fw_mutex_handle_t mutex, uint32_t timeout_ms) {
    (void)mutex;
    (void)timeout_ms;
    return FW_OK;
}

fw_status_t fw_mutex_unlock(fw_mutex_handle_t mutex) {
    (void)mutex;
    return FW_OK;
}

fw_status_t fw_sem_init(fw_sem_handle_t *out_sem, uint32_t init_count, uint32_t max_count) {
    (void)init_count;
    (void)max_count;
    if (out_sem != FW_NULL) *out_sem = (void*)1;
    return FW_OK;
}

fw_status_t fw_sem_take(fw_sem_handle_t sem, uint32_t timeout_ms) {
    (void)sem;
    (void)timeout_ms;
    return FW_OK;
}

fw_status_t fw_sem_give(fw_sem_handle_t sem) {
    (void)sem;
    return FW_OK;
}
