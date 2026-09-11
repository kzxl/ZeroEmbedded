#ifndef ZERO_RTOS_RTOS_H
#define ZERO_RTOS_RTOS_H

/**
 * @file rtos.h
 * @brief Unified RTOS abstraction layer for FreeRTOS, Zephyr, and Baremetal.
 */

#include "../types.h"
#include "../result.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*fw_task_fn_t)(void *arg);

typedef struct {
    const char   *name;
    uint32_t      stack_size;
    uint8_t       priority;
    fw_task_fn_t  entry_point;
    void         *argument;
} fw_task_config_t;

typedef void* fw_task_handle_t;
typedef void* fw_mutex_handle_t;
typedef void* fw_sem_handle_t;

/* Task management */
fw_status_t fw_task_create(const fw_task_config_t *cfg, fw_task_handle_t *out_handle);
FW_NORMAL void fw_task_delay_ms(uint32_t ms);
void fw_task_yield(void);

/* Mutex primitives */
fw_status_t fw_mutex_init(fw_mutex_handle_t *out_mutex);
FW_NORMAL fw_status_t fw_mutex_lock(fw_mutex_handle_t mutex, uint32_t timeout_ms);
fw_status_t fw_mutex_unlock(fw_mutex_handle_t mutex);

/* Semaphore primitives */
fw_status_t fw_sem_init(fw_sem_handle_t *out_sem, uint32_t init_count, uint32_t max_count);
FW_NORMAL fw_status_t fw_sem_take(fw_sem_handle_t sem, uint32_t timeout_ms);
fw_status_t fw_sem_give(fw_sem_handle_t sem);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_RTOS_RTOS_H */
