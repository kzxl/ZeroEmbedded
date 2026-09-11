#ifndef ZERO_SPAN_H
#define ZERO_SPAN_H

/**
 * @file span.h
 * @brief Zero-overhead memory span / slice abstractions for ZeroEmbedded.
 */

#include "types.h"
#include "attributes.h"
#include "assert.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Mutable memory span */
typedef struct {
    void *data;
    fw_size_t length;
} fw_span_t;

/** Const (read-only) memory span */
typedef struct {
    const void *data;
    fw_size_t length;
} fw_cspan_t;

FW_ALWAYS_INLINE fw_span_t fw_span_make(void *data, fw_size_t length) {
    fw_span_t span;
    span.data = data;
    span.length = length;
    return span;
}

FW_ALWAYS_INLINE fw_cspan_t fw_cspan_make(const void *data, fw_size_t length) {
    fw_cspan_t span;
    span.data = data;
    span.length = length;
    return span;
}

FW_ALWAYS_INLINE fw_cspan_t fw_span_to_cspan(fw_span_t span) {
    fw_cspan_t cspan;
    cspan.data = span.data;
    cspan.length = span.length;
    return cspan;
}

FW_ALWAYS_INLINE fw_bool_t fw_span_is_empty(fw_span_t span) {
    return span.data == FW_NULL || span.length == 0;
}

FW_ALWAYS_INLINE fw_bool_t fw_cspan_is_empty(fw_cspan_t span) {
    return span.data == FW_NULL || span.length == 0;
}

FW_ALWAYS_INLINE fw_span_t fw_span_sub(fw_span_t span, fw_size_t offset, fw_size_t length) {
    FW_ASSERT(offset <= span.length);
    FW_ASSERT(offset + length <= span.length);
    fw_span_t sub;
    sub.data = (fw_byte_t*)span.data + offset;
    sub.length = length;
    return sub;
}

FW_ALWAYS_INLINE fw_cspan_t fw_cspan_sub(fw_cspan_t span, fw_size_t offset, fw_size_t length) {
    FW_ASSERT(offset <= span.length);
    FW_ASSERT(offset + length <= span.length);
    fw_cspan_t sub;
    sub.data = (const fw_byte_t*)span.data + offset;
    sub.length = length;
    return sub;
}

#define FW_SPAN_FROM_ARRAY(arr) fw_span_make((void*)(arr), sizeof(arr))
#define FW_CSPAN_FROM_ARRAY(arr) fw_cspan_make((const void*)(arr), sizeof(arr))

#ifdef __cplusplus
}
#endif

#endif /* ZERO_SPAN_H */
