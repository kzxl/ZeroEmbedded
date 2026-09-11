#ifndef ZERO_SYNC_SPSC_H
#define ZERO_SYNC_SPSC_H

/**
 * @file spsc.h
 * @brief Lock-free Single-Producer Single-Consumer (SPSC) ringbuffer for ZeroEmbedded.
 *
 * Safe for concurrent streaming between an Interrupt Service Routine (ISR)
 * and the main thread loop without mutexes or critical section locks.
 */

#include "../types.h"
#include "../result.h"
#include "../span.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t            *buffer;
    fw_size_t           capacity;   /* Must be power of 2 */
    fw_size_t           mask;       /* capacity - 1 */
    volatile fw_size_t  head;       /* Incremented by producer (ISR) */
    volatile fw_size_t  tail;       /* Incremented by consumer (Thread) */
} fw_spsc_t;

/**
 * @brief Initializes an SPSC ringbuffer with a backing array.
 * @param spsc Pointer to SPSC control structure.
 * @param buffer Contiguous memory storage.
 * @param capacity Must be power of 2 (e.g. 64, 128, 256, 1024).
 */
fw_status_t fw_spsc_init(fw_spsc_t *spsc, void *buffer, fw_size_t capacity);

/**
 * @brief Pushes a byte into queue (called by Producer / ISR).
 */
FW_ISR fw_status_t fw_spsc_push(fw_spsc_t *spsc, uint8_t byte);

/**
 * @brief Pops a byte from queue (called by Consumer / Thread).
 */
FW_NORMAL fw_status_t fw_spsc_pop(fw_spsc_t *spsc, uint8_t *out_byte);

/**
 * @brief Pushes a contiguous span of bytes into queue.
 */
FW_ISR fw_size_t fw_spsc_write(fw_spsc_t *spsc, const void *src, fw_size_t count);

/**
 * @brief Reads a contiguous span of bytes from queue.
 */
FW_NORMAL fw_size_t fw_spsc_read(fw_spsc_t *spsc, void *dst, fw_size_t max_count);

/** Returns number of items currently in queue */
FW_INLINE fw_size_t fw_spsc_count(const fw_spsc_t *spsc) {
    if (spsc == FW_NULL) return 0;
    return (spsc->head - spsc->tail);
}

/** Returns remaining capacity in queue */
FW_INLINE fw_size_t fw_spsc_available(const fw_spsc_t *spsc) {
    if (spsc == FW_NULL) return 0;
    return spsc->capacity - (spsc->head - spsc->tail);
}

/** Returns true if queue is empty */
FW_INLINE fw_bool_t fw_spsc_is_empty(const fw_spsc_t *spsc) {
    return spsc != FW_NULL ? (spsc->head == spsc->tail) : FW_TRUE;
}

/** Returns true if queue is full */
FW_INLINE fw_bool_t fw_spsc_is_full(const fw_spsc_t *spsc) {
    return spsc != FW_NULL ? ((spsc->head - spsc->tail) >= spsc->capacity) : FW_TRUE;
}

#ifdef __cplusplus
}
#endif

#endif /* ZERO_SYNC_SPSC_H */
