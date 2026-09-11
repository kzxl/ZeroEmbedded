#include "zero/sync/spsc.h"
#include <string.h>

fw_status_t fw_spsc_init(fw_spsc_t *spsc, void *buffer, fw_size_t capacity) {
    if (spsc == FW_NULL || buffer == FW_NULL || capacity == 0) {
        return FW_ERR_INVALID_ARG;
    }

    /* Capacity must be a power of two */
    if ((capacity & (capacity - 1)) != 0) {
        return FW_ERR_INVALID_ARG;
    }

    spsc->buffer = (uint8_t*)buffer;
    spsc->capacity = capacity;
    spsc->mask = capacity - 1;
    spsc->head = 0;
    spsc->tail = 0;

    return FW_OK;
}

fw_status_t fw_spsc_push(fw_spsc_t *spsc, uint8_t byte) {
    if (spsc == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    fw_size_t head = spsc->head;
    fw_size_t tail = spsc->tail;

    if ((head - tail) >= spsc->capacity) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    spsc->buffer[head & spsc->mask] = byte;
    FW_MEMORY_BARRIER();
    spsc->head = head + 1;

    return FW_OK;
}

fw_status_t fw_spsc_pop(fw_spsc_t *spsc, uint8_t *out_byte) {
    if (spsc == FW_NULL || out_byte == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    fw_size_t tail = spsc->tail;
    FW_MEMORY_BARRIER();
    fw_size_t head = spsc->head;

    if (tail == head) {
        return FW_ERR_NOT_FOUND; /* Empty */
    }

    *out_byte = spsc->buffer[tail & spsc->mask];
    FW_MEMORY_BARRIER();
    spsc->tail = tail + 1;

    return FW_OK;
}

fw_size_t fw_spsc_write(fw_spsc_t *spsc, const void *src, fw_size_t count) {
    if (spsc == FW_NULL || src == FW_NULL || count == 0) {
        return 0;
    }

    const uint8_t *in = (const uint8_t*)src;
    fw_size_t head = spsc->head;
    fw_size_t tail = spsc->tail;
    fw_size_t available = spsc->capacity - (head - tail);
    fw_size_t to_write = count < available ? count : available;
    if (to_write == 0) {
        return 0;
    }

    /* 2-chunk contiguous memory copy */
    fw_size_t idx = head & spsc->mask;
    fw_size_t chunk1 = spsc->capacity - idx;
    if (chunk1 > to_write) {
        chunk1 = to_write;
    }
    memcpy(spsc->buffer + idx, in, chunk1);

    fw_size_t chunk2 = to_write - chunk1;
    if (chunk2 > 0) {
        memcpy(spsc->buffer, in + chunk1, chunk2);
    }

    FW_MEMORY_BARRIER();
    spsc->head = head + to_write;
    return to_write;
}

fw_size_t fw_spsc_read(fw_spsc_t *spsc, void *dst, fw_size_t max_count) {
    if (spsc == FW_NULL || dst == FW_NULL || max_count == 0) {
        return 0;
    }

    uint8_t *out = (uint8_t*)dst;
    fw_size_t tail = spsc->tail;
    FW_MEMORY_BARRIER();
    fw_size_t head = spsc->head;
    fw_size_t count = (head - tail);
    fw_size_t to_read = max_count < count ? max_count : count;
    if (to_read == 0) {
        return 0;
    }

    /* 2-chunk contiguous memory copy */
    fw_size_t idx = tail & spsc->mask;
    fw_size_t chunk1 = spsc->capacity - idx;
    if (chunk1 > to_read) {
        chunk1 = to_read;
    }
    memcpy(out, spsc->buffer + idx, chunk1);

    fw_size_t chunk2 = to_read - chunk1;
    if (chunk2 > 0) {
        memcpy(out + chunk1, spsc->buffer, chunk2);
    }

    FW_MEMORY_BARRIER();
    spsc->tail = tail + to_read;
    return to_read;
}
