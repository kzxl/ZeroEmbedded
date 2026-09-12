#include "zero/rtos/monitor.h"
#include <string.h>

static fw_task_metric_t s_metrics[FW_MONITOR_MAX_TASKS];
static fw_size_t s_metric_count = 0;

fw_status_t fw_task_stack_paint(void *stack_base, fw_size_t stack_size) {
    if (stack_base == FW_NULL || stack_size < sizeof(uint32_t)) {
        return FW_ERR_INVALID_ARG;
    }

    uint32_t *words = (uint32_t*)stack_base;
    fw_size_t count = stack_size / sizeof(uint32_t);

    for (fw_size_t i = 0; i < count; i++) {
        words[i] = FW_STACK_PAINT_CANARY;
    }

    FW_MEMORY_BARRIER();
    return FW_OK;
}

fw_size_t fw_task_stack_unused(const void *stack_base, fw_size_t stack_size) {
    if (stack_base == FW_NULL || stack_size < sizeof(uint32_t)) {
        return 0;
    }

    const uint32_t *words = (const uint32_t*)stack_base;
    fw_size_t count = stack_size / sizeof(uint32_t);
    fw_size_t unused_words = 0;

    /* Most architectures grow stack downwards: bottom is at index 0 */
    for (fw_size_t i = 0; i < count; i++) {
        if (words[i] == FW_STACK_PAINT_CANARY) {
            unused_words++;
        } else {
            break;
        }
    }

    return unused_words * sizeof(uint32_t);
}

fw_size_t fw_task_stack_peak(const void *stack_base, fw_size_t stack_size) {
    fw_size_t unused = fw_task_stack_unused(stack_base, stack_size);
    return (unused >= stack_size) ? 0 : (stack_size - unused);
}

fw_status_t fw_task_monitor_record(uint32_t task_id, uint32_t runtime_us, fw_size_t peak_stack) {
    for (fw_size_t i = 0; i < s_metric_count; i++) {
        if (s_metrics[i].task_id == task_id) {
            s_metrics[i].total_runtime_us += runtime_us;
            s_metrics[i].execution_count++;
            if (peak_stack > s_metrics[i].stack_peak_bytes) {
                s_metrics[i].stack_peak_bytes = peak_stack;
            }
            return FW_OK;
        }
    }

    if (s_metric_count >= FW_MONITOR_MAX_TASKS) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    fw_task_metric_t *m = &s_metrics[s_metric_count++];
    m->task_id = task_id;
    m->total_runtime_us = runtime_us;
    m->execution_count = 1;
    m->stack_peak_bytes = peak_stack;
    m->stack_size = 0;

    return FW_OK;
}

fw_status_t fw_task_monitor_get(uint32_t task_id, fw_task_metric_t *out_metric) {
    if (out_metric == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    for (fw_size_t i = 0; i < s_metric_count; i++) {
        if (s_metrics[i].task_id == task_id) {
            *out_metric = s_metrics[i];
            return FW_OK;
        }
    }

    return FW_ERR_NOT_FOUND;
}

void fw_task_monitor_reset(void) {
    memset(s_metrics, 0, sizeof(s_metrics));
    s_metric_count = 0;
}
