#ifndef ZERO_DIAG_LOG_H
#define ZERO_DIAG_LOG_H

/**
 * @file log.h
 * @brief Deferred Lock-Free Binary Logger for Real-Time Embedded Systems
 * Decouples fast diagnostic event logging from slow UART/communication sinks.
 * Pushes fixed-size log entries into an SPSC lock-free queue in < 25 ns,
 * enabling safe logging inside ISR and hard real-time superloops without blocking.
 */

#include "zero/types.h"
#include "zero/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_LOG_LEVEL_NONE  0
#define FW_LOG_LEVEL_ERROR 1
#define FW_LOG_LEVEL_WARN  2
#define FW_LOG_LEVEL_INFO  3
#define FW_LOG_LEVEL_DEBUG 4

#ifndef FW_LOG_LEVEL
#define FW_LOG_LEVEL FW_LOG_LEVEL_DEBUG
#endif

typedef void (*fw_log_sink_fn_t)(const char *str, fw_size_t len);

typedef struct {
    uint32_t    timestamp_ms;
    const char *tag;
    const char *fmt;
    uintptr_t   arg1;
    uintptr_t   arg2;
    uint8_t     level;
} fw_log_entry_t;

/**
 * @brief Initializes the deferred logging subsystem with static queue storage.
 *
 * @param sink Output function to stream formatted text (e.g. UART transmit callback).
 * @param storage Statically allocated array of fw_log_entry_t.
 * @param capacity Number of entries in storage (power-of-two recommended).
 * @return FW_OK on success, FW_ERR_INVALID_ARG on NULL pointers.
 */
fw_status_t fw_log_init(fw_log_sink_fn_t sink, fw_log_entry_t *storage, fw_size_t capacity);

/**
 * @brief Posts an event to the lock-free log queue. Safe to call inside ISRs.
 */
void fw_log_post_fast(uint8_t level, const char *tag, const char *fmt, uintptr_t arg1, uintptr_t arg2);

/**
 * @brief Drains pending log entries, formats them, and dispatches them to the sink.
 * Must be called in the main superloop or low-priority background task.
 *
 * @param max_entries Maximum entries to flush per call (0 = flush all pending).
 * @return Number of log entries flushed.
 */
fw_size_t fw_log_flush(fw_size_t max_entries);

/* Compile-time level stripped logging macros */
#if (FW_LOG_LEVEL >= FW_LOG_LEVEL_ERROR)
#define FW_LOG_E(tag, fmt, ...) fw_log_post_fast(FW_LOG_LEVEL_ERROR, tag, fmt, (uintptr_t)(0, ##__VA_ARGS__))
#else
#define FW_LOG_E(tag, fmt, ...) ((void)0)
#endif

#if (FW_LOG_LEVEL >= FW_LOG_LEVEL_WARN)
#define FW_LOG_W(tag, fmt, ...) fw_log_post_fast(FW_LOG_LEVEL_WARN, tag, fmt, (uintptr_t)(0, ##__VA_ARGS__))
#else
#define FW_LOG_W(tag, fmt, ...) ((void)0)
#endif

#if (FW_LOG_LEVEL >= FW_LOG_LEVEL_INFO)
#define FW_LOG_I(tag, fmt, ...) fw_log_post_fast(FW_LOG_LEVEL_INFO, tag, fmt, (uintptr_t)(0, ##__VA_ARGS__))
#else
#define FW_LOG_I(tag, fmt, ...) ((void)0)
#endif

#if (FW_LOG_LEVEL >= FW_LOG_LEVEL_DEBUG)
#define FW_LOG_D(tag, fmt, ...) fw_log_post_fast(FW_LOG_LEVEL_DEBUG, tag, fmt, (uintptr_t)(0, ##__VA_ARGS__))
#else
#define FW_LOG_D(tag, fmt, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZERO_DIAG_LOG_H */
