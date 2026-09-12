#ifndef ZERO_RTOS_MONITOR_H
#define ZERO_RTOS_MONITOR_H

/**
 * @file monitor.h
 * @brief RTOS Task High-Water Mark Stack Profiler & CPU Load Monitor.
 * Inspired by Magic Lantern's tskmon execution analyzer.
 */

#include "../types.h"
#include "../result.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_STACK_PAINT_CANARY 0x5A5A5A5AU
#define FW_MONITOR_MAX_TASKS  16

typedef struct {
    uint32_t task_id;
    uint32_t total_runtime_us;
    uint32_t execution_count;
    fw_size_t stack_size;
    fw_size_t stack_peak_bytes;
} fw_task_metric_t;

/**
 * @brief Paints a task stack with a watermark canary pattern before execution.
 */
fw_status_t fw_task_stack_paint(void *stack_base, fw_size_t stack_size);

/**
 * @brief Calculates the peak stack consumption (high-water mark) in bytes.
 * Scans for the boundary of uncorrupted canary pattern words.
 */
fw_size_t fw_task_stack_peak(const void *stack_base, fw_size_t stack_size);

/**
 * @brief Calculates remaining unused stack headroom in bytes.
 */
fw_size_t fw_task_stack_unused(const void *stack_base, fw_size_t stack_size);

/**
 * @brief Records runtime metrics for a task execution slice.
 */
fw_status_t fw_task_monitor_record(uint32_t task_id, uint32_t runtime_us, fw_size_t peak_stack);

/**
 * @brief Retrieves recorded metrics for a specific task.
 */
fw_status_t fw_task_monitor_get(uint32_t task_id, fw_task_metric_t *out_metric);

/**
 * @brief Resets all task monitor metrics.
 */
void fw_task_monitor_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_RTOS_MONITOR_H */
