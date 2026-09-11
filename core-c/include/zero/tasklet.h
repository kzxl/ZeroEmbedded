#ifndef ZERO_TASKLET_H
#define ZERO_TASKLET_H

/**
 * @file tasklet.h
 * @brief Lock-free, zero-heap cooperative tasklet / deferred execution dispatcher.
 *
 * Enables Interrupt Service Routines (ISRs) to post deferred work items
 * to the main superloop without dynamic memory allocation or mutex locking.
 */

#include "types.h"
#include "result.h"
#include "attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*fw_tasklet_handler_t)(void *context, uint32_t arg);

typedef struct {
    fw_tasklet_handler_t handler;
    void                *context;
    uint32_t             arg;
} fw_tasklet_item_t;

typedef struct {
    fw_tasklet_item_t  *storage;
    fw_size_t           capacity;   /* Must be power of 2 */
    fw_size_t           mask;
    volatile fw_size_t  head;       /* Incremented by Producer (ISR) */
    volatile fw_size_t  tail;       /* Incremented by Consumer (Main thread) */
} fw_tasklet_queue_t;

/**
 * @brief Initializes a tasklet queue backed by static array storage.
 * @param q Pointer to tasklet queue structure.
 * @param storage Contiguous array of fw_tasklet_item_t.
 * @param capacity Number of items (must be power of 2, e.g. 8, 16, 32, 64).
 */
fw_status_t fw_tasklet_queue_init(
    fw_tasklet_queue_t *q,
    fw_tasklet_item_t *storage,
    fw_size_t capacity
);

/**
 * @brief Posts a deferred work item (callable safely from ISR).
 */
FW_ISR fw_status_t fw_tasklet_post(
    fw_tasklet_queue_t *q,
    fw_tasklet_handler_t handler,
    void *context,
    uint32_t arg
);

/**
 * @brief Dispatches and executes the next pending tasklet.
 * @return FW_TRUE if a tasklet was executed, FW_FALSE if queue is empty.
 */
FW_NORMAL fw_bool_t fw_tasklet_dispatch_one(fw_tasklet_queue_t *q);

/**
 * @brief Dispatches and executes all currently queued tasklets.
 * @return Number of tasklets executed.
 */
FW_NORMAL fw_size_t fw_tasklet_dispatch_all(fw_tasklet_queue_t *q);

/** Returns number of pending tasklets in queue */
FW_INLINE fw_size_t fw_tasklet_count(const fw_tasklet_queue_t *q) {
    if (q == FW_NULL) return 0;
    return (q->head - q->tail);
}

/** Returns true if queue has no pending work */
FW_INLINE fw_bool_t fw_tasklet_is_empty(const fw_tasklet_queue_t *q) {
    return q != FW_NULL ? (q->head == q->tail) : FW_TRUE;
}

#ifdef __cplusplus
}
#endif

#endif /* ZERO_TASKLET_H */
