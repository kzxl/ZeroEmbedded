#ifndef ZERO_HAL_TIMER_H
#define ZERO_HAL_TIMER_H

/**
 * @file timer.h
 * @brief High-precision hardware timer and tick counters for ZeroEmbedded.
 */

#include "../types.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Returns monotonic uptime in milliseconds */
uint32_t fw_timer_get_millis(void);

/** Returns monotonic uptime in microseconds (high resolution) */
uint64_t fw_timer_get_micros(void);

/** Busy-wait delay in microseconds */
void fw_delay_micros(uint32_t us);

/** Busy-wait delay in milliseconds (Prohibited in FW_ISR) */
FW_NORMAL void fw_delay_millis(uint32_t ms);

/** Non-blocking software timeout structure (Zero dynamic heap) */
typedef struct {
    uint32_t start_ms;
    uint32_t duration_ms;
} fw_timeout_t;

FW_INLINE void fw_timeout_start(fw_timeout_t *t, uint32_t duration_ms) {
    if (t != FW_NULL) {
        t->start_ms = fw_timer_get_millis();
        t->duration_ms = duration_ms;
    }
}

FW_INLINE fw_bool_t fw_timeout_is_expired(const fw_timeout_t *t) {
    if (t == FW_NULL) return FW_TRUE;
    uint32_t elapsed = fw_timer_get_millis() - t->start_ms;
    return elapsed >= t->duration_ms ? FW_TRUE : FW_FALSE;
}

FW_INLINE uint32_t fw_timeout_remaining_ms(const fw_timeout_t *t) {
    if (t == FW_NULL) return 0;
    uint32_t elapsed = fw_timer_get_millis() - t->start_ms;
    return (elapsed < t->duration_ms) ? (t->duration_ms - elapsed) : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_TIMER_H */
