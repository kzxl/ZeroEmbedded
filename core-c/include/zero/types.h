#ifndef ZERO_TYPES_H
#define ZERO_TYPES_H

/**
 * @file types.h
 * @brief Core primitive type definitions for ZeroEmbedded.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool            fw_bool_t;
#define FW_TRUE         true
#define FW_FALSE        false

#ifndef FW_NULL
#define FW_NULL         ((void*)0)
#endif

typedef size_t          fw_size_t;
typedef ptrdiff_t       fw_ssize_t;
typedef uintptr_t       fw_uintptr_t;
typedef intptr_t        fw_intptr_t;
typedef uint8_t         fw_byte_t;

/* Explicit bitwidth aliases */
typedef uint8_t         fw_u8_t;
typedef uint16_t        fw_u16_t;
typedef uint32_t        fw_u32_t;
typedef uint64_t        fw_u64_t;

typedef int8_t          fw_i8_t;
typedef int16_t         fw_i16_t;
typedef int32_t         fw_i32_t;
typedef int64_t         fw_i64_t;

typedef float           fw_f32_t;
typedef double          fw_f64_t;

#ifdef __cplusplus
}
#endif

#endif /* ZERO_TYPES_H */
