/**
 * @file filter.c
 * @brief Digital Signal Processing & Sensor Filtering implementation
 */

#include "zero/dsp/filter.h"

#define SWAP_IF_GREATER(a, b) \
    do { \
        if ((a) > (b)) { \
            uint16_t _tmp = (a); \
            (a) = (b); \
            (b) = _tmp; \
        } \
    } while (0)

uint16_t fw_filter_median5_u16(uint16_t s0, uint16_t s1, uint16_t s2, uint16_t s3, uint16_t s4) {
    SWAP_IF_GREATER(s0, s1);
    SWAP_IF_GREATER(s2, s3);
    SWAP_IF_GREATER(s0, s2);
    SWAP_IF_GREATER(s1, s3);
    SWAP_IF_GREATER(s1, s2);
    SWAP_IF_GREATER(s0, s4);
    SWAP_IF_GREATER(s1, s4);
    SWAP_IF_GREATER(s2, s4);
    SWAP_IF_GREATER(s2, s3);

    return s2;
}

void fw_debounce_init(fw_debounce_t *db, uint16_t threshold, fw_bool_t initial_state) {
    if (db == FW_NULL) {
        return;
    }
    db->counter         = 0;
    db->threshold       = (threshold > 0) ? threshold : 1;
    db->debounced_state = initial_state;
}

fw_bool_t fw_debounce_update(fw_debounce_t *db, fw_bool_t raw_sample) {
    if (db == FW_NULL) {
        return FW_FALSE;
    }

    if (raw_sample == db->debounced_state) {
        db->counter = 0;
        return FW_FALSE;
    }

    db->counter++;
    if (db->counter >= db->threshold) {
        db->debounced_state = raw_sample;
        db->counter         = 0;
        return FW_TRUE; /* Transition occurred */
    }

    return FW_FALSE;
}
