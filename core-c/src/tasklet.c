#include "zero/tasklet.h"
#include <string.h>

fw_status_t fw_tasklet_queue_init(
    fw_tasklet_queue_t *q,
    fw_tasklet_item_t *storage,
    fw_size_t capacity
) {
    if (q == FW_NULL || storage == FW_NULL || capacity == 0) {
        return FW_ERR_INVALID_ARG;
    }

    /* Capacity must be a power of two */
    if ((capacity & (capacity - 1)) != 0) {
        return FW_ERR_INVALID_ARG;
    }

    q->storage = storage;
    q->capacity = capacity;
    q->mask = capacity - 1;
    q->head = 0;
    q->tail = 0;

    return FW_OK;
}

fw_status_t fw_tasklet_post(
    fw_tasklet_queue_t *q,
    fw_tasklet_handler_t handler,
    void *context,
    uint32_t arg
) {
    if (q == FW_NULL || handler == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    fw_size_t head = q->head;
    fw_size_t tail = q->tail;

    if ((head - tail) >= q->capacity) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    fw_size_t idx = head & q->mask;
    q->storage[idx].handler = handler;
    q->storage[idx].context = context;
    q->storage[idx].arg = arg;

    FW_MEMORY_BARRIER();
    q->head = head + 1;

    return FW_OK;
}

fw_bool_t fw_tasklet_dispatch_one(fw_tasklet_queue_t *q) {
    if (q == FW_NULL) {
        return FW_FALSE;
    }

    fw_size_t tail = q->tail;
    FW_MEMORY_BARRIER();
    fw_size_t head = q->head;

    if (tail == head) {
        return FW_FALSE; /* Queue empty */
    }

    fw_size_t idx = tail & q->mask;
    fw_tasklet_handler_t handler = q->storage[idx].handler;
    void *context = q->storage[idx].context;
    uint32_t arg = q->storage[idx].arg;

    FW_MEMORY_BARRIER();
    q->tail = tail + 1;

    if (handler != FW_NULL) {
        handler(context, arg);
    }

    return FW_TRUE;
}

fw_size_t fw_tasklet_dispatch_all(fw_tasklet_queue_t *q) {
    if (q == FW_NULL) {
        return 0;
    }

    fw_size_t count = 0;
    while (fw_tasklet_dispatch_one(q)) {
        count++;
    }

    return count;
}
