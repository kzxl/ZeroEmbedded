/**
 * @file log.c
 * @brief Deferred Lock-Free Binary Logger implementation
 */

#include "zero/diag/log.h"
#include "zero/hal/timer.h"
#include "zero/attributes.h"
#include <stdio.h>
#include <string.h>

static fw_log_sink_fn_t s_sink = FW_NULL;
static fw_log_entry_t *s_entries = FW_NULL;
static fw_size_t s_capacity = 0;
static volatile fw_size_t s_head = 0;
static volatile fw_size_t s_tail = 0;

static void default_stdout_sink(const char *str, fw_size_t len) {
    (void)len;
    fputs(str, stdout);
}

fw_status_t fw_log_init(fw_log_sink_fn_t sink, fw_log_entry_t *storage, fw_size_t capacity) {
    if (storage == FW_NULL || capacity < 2) {
        return FW_ERR_INVALID_ARG;
    }

    s_sink     = (sink != FW_NULL) ? sink : default_stdout_sink;
    s_entries  = storage;
    s_capacity = capacity;
    s_head     = 0;
    s_tail     = 0;

    return FW_OK;
}

void fw_log_post_fast(uint8_t level, const char *tag, const char *fmt, uintptr_t arg1, uintptr_t arg2) {
    if (s_entries == FW_NULL || s_capacity == 0) {
        return;
    }

    fw_size_t next_head = (s_head + 1) % s_capacity;
    if (next_head == s_tail) {
        /* Queue full -> drop entry without blocking */
        return;
    }

    fw_log_entry_t *entry = &s_entries[s_head];
    entry->timestamp_ms = fw_timer_get_millis();
    entry->level        = level;
    entry->tag          = (tag != FW_NULL) ? tag : "SYS";
    entry->fmt          = (fmt != FW_NULL) ? fmt : "";
    entry->arg1         = arg1;
    entry->arg2         = arg2;

    FW_MEMORY_BARRIER();
    s_head = next_head;
}

fw_size_t fw_log_flush(fw_size_t max_entries) {
    if (s_entries == FW_NULL || s_sink == FW_NULL) {
        return 0;
    }

    fw_size_t flushed = 0;
    char line_buf[256];
    char msg_buf[192];

    while (s_tail != s_head) {
        if (max_entries > 0 && flushed >= max_entries) {
            break;
        }

        const fw_log_entry_t *entry = &s_entries[s_tail];

        /* Format message payload with up to 2 numeric/pointer arguments */
        snprintf(msg_buf, sizeof(msg_buf), entry->fmt, entry->arg1, entry->arg2);

        /* Map log level to color & tag */
        const char *lvl_color = "\033[0m[?]";
        switch (entry->level) {
            case FW_LOG_LEVEL_ERROR: lvl_color = "\033[1;31m[E]"; break;
            case FW_LOG_LEVEL_WARN:  lvl_color = "\033[1;33m[W]"; break;
            case FW_LOG_LEVEL_INFO:  lvl_color = "\033[1;32m[I]"; break;
            case FW_LOG_LEVEL_DEBUG: lvl_color = "\033[1;36m[D]"; break;
            default: break;
        }

        int len = snprintf(line_buf, sizeof(line_buf), "%s %05u][%s] %s\033[0m\n",
                           lvl_color, entry->timestamp_ms, entry->tag, msg_buf);

        if (len > 0) {
            s_sink(line_buf, (fw_size_t)len);
        }

        FW_MEMORY_BARRIER();
        s_tail = (s_tail + 1) % s_capacity;
        flushed++;
    }

    return flushed;
}
