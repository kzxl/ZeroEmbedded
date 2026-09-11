#ifndef ZERO_SPAN_H
#define ZERO_SPAN_H

/**
 * @file span.h
 * @brief Zero-overhead memory span / slice abstractions for ZeroEmbedded.
 */

#include "types.h"
#include "attributes.h"
#include "assert.h"
#include <string.h>

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
    FW_ASSERT(length <= span.length - offset);
    fw_span_t sub;
    sub.data = (fw_byte_t*)span.data + offset;
    sub.length = length;
    return sub;
}

FW_ALWAYS_INLINE fw_cspan_t fw_cspan_sub(fw_cspan_t span, fw_size_t offset, fw_size_t length) {
    FW_ASSERT(offset <= span.length);
    FW_ASSERT(length <= span.length - offset);
    fw_cspan_t sub;
    sub.data = (const fw_byte_t*)span.data + offset;
    sub.length = length;
    return sub;
}

FW_ALWAYS_INLINE fw_status_t fw_span_sub_safe(fw_span_t span, fw_size_t offset, fw_size_t length, fw_span_t *out_sub) {
    if (out_sub == FW_NULL) return FW_ERR_INVALID_ARG;
    if (offset > span.length || length > span.length - offset) {
        *out_sub = fw_span_make(FW_NULL, 0);
        return FW_ERR_BUFFER_OVERFLOW;
    }
    out_sub->data = (fw_byte_t*)span.data + offset;
    out_sub->length = length;
    return FW_OK;
}

FW_ALWAYS_INLINE fw_status_t fw_cspan_sub_safe(fw_cspan_t span, fw_size_t offset, fw_size_t length, fw_cspan_t *out_sub) {
    if (out_sub == FW_NULL) return FW_ERR_INVALID_ARG;
    if (offset > span.length || length > span.length - offset) {
        *out_sub = fw_cspan_make(FW_NULL, 0);
        return FW_ERR_BUFFER_OVERFLOW;
    }
    out_sub->data = (const fw_byte_t*)span.data + offset;
    out_sub->length = length;
    return FW_OK;
}

FW_ALWAYS_INLINE fw_size_t fw_span_copy(fw_span_t dst, fw_cspan_t src) {
    if (dst.data == FW_NULL || src.data == FW_NULL) return 0;
    fw_size_t to_copy = dst.length < src.length ? dst.length : src.length;
    if (to_copy > 0) {
        memcpy(dst.data, src.data, to_copy);
    }
    return to_copy;
}

FW_ALWAYS_INLINE void fw_span_fill(fw_span_t dst, uint8_t value) {
    if (dst.data != FW_NULL && dst.length > 0) {
        memset(dst.data, value, dst.length);
    }
}

#define FW_SPAN_FROM_ARRAY(arr) fw_span_make((void*)(arr), sizeof(arr))
#define FW_CSPAN_FROM_ARRAY(arr) fw_cspan_make((const void*)(arr), sizeof(arr))

#ifdef __cplusplus
}
#endif

#endif /* ZERO_SPAN_H */
