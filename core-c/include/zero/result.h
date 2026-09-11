#ifndef ZERO_RESULT_H
#define ZERO_RESULT_H

/**
 * @file result.h
 * @brief Error codes, status types, and propagation macros for ZeroEmbedded.
 */

#include "types.h"
#include "attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FW_OK                   = 0,
    FW_ERR_GENERIC          = -1,
    FW_ERR_INVALID_ARG      = -2,
    FW_ERR_OUT_OF_MEMORY    = -3,
    FW_ERR_BUFFER_OVERFLOW  = -4,
    FW_ERR_TIMEOUT          = -5,
    FW_ERR_BUSY             = -6,
    FW_ERR_NOT_FOUND        = -7,
    FW_ERR_NOT_SUPPORTED    = -8,
    FW_ERR_IO               = -9,
    FW_ERR_CORRUPTED        = -10,
    FW_ERR_ISR_VIOLATION    = -11,
    FW_ERR_DMA_UNALIGNED    = -12
} fw_status_t;

FW_INLINE fw_bool_t fw_is_ok(fw_status_t status) {
    return status == FW_OK;
}

FW_INLINE fw_bool_t fw_is_err(fw_status_t status) {
    return status != FW_OK;
}

/** Propagation macro: returns early if status is not FW_OK */
#define FW_CHECK(expr) \
    do { \
        fw_status_t _st = (expr); \
        if (FW_UNLIKELY(_st != FW_OK)) { \
            return _st; \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* ZERO_RESULT_H */
