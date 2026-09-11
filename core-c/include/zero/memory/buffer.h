#ifndef ZERO_MEMORY_BUFFER_H
#define ZERO_MEMORY_BUFFER_H

/**
 * @file buffer.h
 * @brief Safe bounded byte buffer with lifetime and bounds checking for ZeroEmbedded.
 *
 * Encapsulates address, capacity, length, and bounds checking to eliminate
 * raw pointer + size arithmetic errors. Backing storage is strictly non-heap.
 */

#include "../types.h"
#include "../result.h"
#include "../span.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t   *data;
    fw_size_t  capacity;
    fw_size_t  length;
} fw_buffer_t;

/**
 * @brief Initializes a bounded buffer with explicit backing storage.
 */
fw_status_t fw_buffer_init(fw_buffer_t *buf, void *storage, fw_size_t capacity);

/**
 * @brief Appends raw bytes to the buffer with bounds checking.
 */
fw_status_t fw_buffer_append(fw_buffer_t *buf, const void *src, fw_size_t count);

FW_ALWAYS_INLINE fw_status_t fw_buffer_append_byte(fw_buffer_t *buf, uint8_t byte) {
    if (FW_UNLIKELY(buf == FW_NULL)) {
        return FW_ERR_INVALID_ARG;
    }
    if (FW_UNLIKELY(buf->length >= buf->capacity)) {
        return FW_ERR_BUFFER_OVERFLOW;
    }
    buf->data[buf->length++] = byte;
    return FW_OK;
}

/**
 * @brief Appends a memory span to the buffer.
 */
FW_ALWAYS_INLINE fw_status_t fw_buffer_append_span(fw_buffer_t *buf, fw_cspan_t span) {
    return fw_buffer_append(buf, span.data, span.length);
}

/** Returns the active contents as a mutable span */
FW_ALWAYS_INLINE fw_span_t fw_buffer_as_span(fw_buffer_t *buf) {
    if (buf == FW_NULL || buf->data == FW_NULL) {
        return fw_span_make(FW_NULL, 0);
    }
    return fw_span_make(buf->data, buf->length);
}

/** Returns the active contents as a const span */
FW_ALWAYS_INLINE fw_cspan_t fw_buffer_as_cspan(const fw_buffer_t *buf) {
    if (buf == FW_NULL || buf->data == FW_NULL) {
        return fw_cspan_make(FW_NULL, 0);
    }
    return fw_cspan_make(buf->data, buf->length);
}

/** Clears buffer contents (resets length to 0) */
FW_ALWAYS_INLINE void fw_buffer_clear(fw_buffer_t *buf) {
    if (buf != FW_NULL) {
        buf->length = 0;
    }
}

/** Returns remaining available capacity in bytes */
FW_ALWAYS_INLINE fw_size_t fw_buffer_remaining(const fw_buffer_t *buf) {
    return (buf != FW_NULL && buf->capacity >= buf->length) ?
           (buf->capacity - buf->length) : 0;
}

/** Checks if buffer has reached capacity */
FW_ALWAYS_INLINE fw_bool_t fw_buffer_is_full(const fw_buffer_t *buf) {
    return buf != FW_NULL ? (buf->length >= buf->capacity) : FW_TRUE;
}

/** Checks if buffer is empty */
FW_ALWAYS_INLINE fw_bool_t fw_buffer_is_empty(const fw_buffer_t *buf) {
    return buf != FW_NULL ? (buf->length == 0) : FW_TRUE;
}

/** Helper to declare a static buffer and its backing array together */
#define FW_DECLARE_STATIC_BUFFER(name, cap) \
    static uint8_t name##_storage[(cap)]; \
    static fw_buffer_t name = { name##_storage, (cap), 0 }

#ifdef __cplusplus
}
#endif

#endif /* ZERO_MEMORY_BUFFER_H */
