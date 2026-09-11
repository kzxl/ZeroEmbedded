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

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_TIMER_H */
