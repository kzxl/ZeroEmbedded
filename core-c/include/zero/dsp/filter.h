#ifndef ZERO_DSP_FILTER_H
#define ZERO_DSP_FILTER_H

/**
 * @file filter.h
 * @brief Fixed-Point Digital Signal Processing & Sensor Filtering
 * Zero-float, deterministic primitives for ADC smoothing, spike rejection, and GPIO debounce.
 */

#include "zero/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* 1. Exponential Moving Average (EMA) Fixed-Point Filters                    */
/* ========================================================================== */

/**
 * @brief Computes 16-bit unsigned EMA using bit-shift division (Zero-Float).
 * Formula: Y[n] = Y[n-1] + ((X[n] - Y[n-1]) >> alpha_shift)
 *
 * @param current_ema Previous filtered value Y[n-1].
 * @param new_sample  New raw sensor sample X[n].
 * @param alpha_shift Power-of-two filter coefficient (e.g. 1=50%, 2=25%, 3=12.5%, 4=6.25%).
 * @return Filtered output Y[n].
 */
static inline uint16_t fw_filter_ema_u16(uint16_t current_ema, uint16_t new_sample, uint8_t alpha_shift) {
    int32_t diff = (int32_t)new_sample - (int32_t)current_ema;
    return (uint16_t)((int32_t)current_ema + (diff >> alpha_shift));
}

/**
 * @brief Computes 32-bit signed EMA using bit-shift division.
 */
static inline int32_t fw_filter_ema_i32(int32_t current_ema, int32_t new_sample, uint8_t alpha_shift) {
    int64_t diff = (int64_t)new_sample - (int64_t)current_ema;
    return (int32_t)((int64_t)current_ema + (diff >> alpha_shift));
}

/* ========================================================================== */
/* 2. Median Filters (Spike / Outlier Noise Rejection)                         */
/* ========================================================================== */

/**
 * @brief Computes median of 3 samples (removes isolated ADC noise spikes).
 */
static inline uint16_t fw_filter_median3_u16(uint16_t a, uint16_t b, uint16_t c) {
    if ((a <= b) && (b <= c)) return b;
    if ((a <= c) && (c <= b)) return c;
    if ((b <= a) && (a <= c)) return a;
    if ((b <= c) && (c <= a)) return c;
    if ((c <= a) && (a <= b)) return a;
    return b;
}

/**
 * @brief Computes median of 5 samples using an optimal sorting network.
 */
uint16_t fw_filter_median5_u16(uint16_t s0, uint16_t s1, uint16_t s2, uint16_t s3, uint16_t s4);

/* ========================================================================== */
/* 3. GPIO & Mechanical Switch Debouncer                                      */
/* ========================================================================== */

typedef struct {
    uint16_t  counter;          /*!< Integrator count */
    uint16_t  threshold;        /*!< Stable count required to flip state */
    fw_bool_t debounced_state;  /*!< Current debounced state */
} fw_debounce_t;

/**
 * @brief Initializes a debouncer instance.
 *
 * @param db Pointer to debouncer.
 * @param threshold Number of consecutive identical samples to confirm state transition.
 * @param initial_state Initial resting state (e.g. FW_FALSE for active-high released).
 */
void fw_debounce_init(fw_debounce_t *db, uint16_t threshold, fw_bool_t initial_state);

/**
 * @brief Updates debouncer with a new raw sample (called periodically in superloop).
 *
 * @param db Pointer to debouncer.
 * @param raw_sample Raw pin level read from GPIO.
 * @return True if state transitioned on this update, False if state unchanged.
 */
fw_bool_t fw_debounce_update(fw_debounce_t *db, fw_bool_t raw_sample);

/**
 * @brief Returns current stable debounced state.
 */
static inline fw_bool_t fw_debounce_is_active(const fw_debounce_t *db) {
    return (db != FW_NULL) ? db->debounced_state : FW_FALSE;
}

#ifdef __cplusplus
}
#endif

#endif /* ZERO_DSP_FILTER_H */
