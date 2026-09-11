#ifndef ZERO_STRING_VIEW_H
#define ZERO_STRING_VIEW_H

/**
 * @file string_view.h
 * @brief Zero-allocation string view slice for ZeroEmbedded.
 */

#include "types.h"
#include "attributes.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *data;
    fw_size_t length;
} fw_string_view_t;

FW_ALWAYS_INLINE fw_string_view_t fw_sv_make(const char *data, fw_size_t length) {
    fw_string_view_t sv;
    sv.data = data;
    sv.length = length;
    return sv;
}

FW_ALWAYS_INLINE fw_string_view_t fw_sv_from_cstr(const char *cstr) {
    if (cstr == FW_NULL) {
        return fw_sv_make(FW_NULL, 0);
    }
    return fw_sv_make(cstr, strlen(cstr));
}

#define FW_SV_LITERAL(str) fw_sv_make((str), sizeof(str) - 1)

FW_ALWAYS_INLINE fw_bool_t fw_sv_is_empty(fw_string_view_t sv) {
    return sv.data == FW_NULL || sv.length == 0;
}

FW_ALWAYS_INLINE fw_bool_t fw_sv_equals(fw_string_view_t a, fw_string_view_t b) {
    if (a.length != b.length) return FW_FALSE;
    if (a.data == b.data) return FW_TRUE;
    if (a.data == FW_NULL || b.data == FW_NULL) return FW_FALSE;
    return memcmp(a.data, b.data, a.length) == 0 ? FW_TRUE : FW_FALSE;
}

#ifdef __cplusplus
}
#endif

#endif /* ZERO_STRING_VIEW_H */
