#include "zero/init.h"
#include <string.h>

static fw_init_entry_t s_init_entries[FW_INIT_MAX_ENTRIES];
static fw_size_t s_init_count = 0;
static fw_bool_t s_level_executed[FW_INIT_LEVEL_COUNT] = {FW_FALSE, FW_FALSE, FW_FALSE, FW_FALSE};

fw_status_t fw_init_register(const char *name, fw_init_fn_t fn, uint8_t level, uint8_t priority) {
    if (fn == FW_NULL || level >= FW_INIT_LEVEL_COUNT) {
        return FW_ERR_INVALID_ARG;
    }

    if (s_init_count >= FW_INIT_MAX_ENTRIES) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    /* Check for duplicate registration */
    for (fw_size_t i = 0; i < s_init_count; i++) {
        if (s_init_entries[i].init_fn == fn) {
            return FW_OK; /* Already registered */
        }
    }

    s_init_entries[s_init_count].name = name ? name : "unnamed_init";
    s_init_entries[s_init_count].init_fn = fn;
    s_init_entries[s_init_count].level = level;
    s_init_entries[s_init_count].priority = priority;
    s_init_count++;

    return FW_OK;
}

fw_status_t fw_init_level(fw_init_level_t level) {
    if (level >= FW_INIT_LEVEL_COUNT) {
        return FW_ERR_INVALID_ARG;
    }

    /* Bubble sort registered entries in this level by priority */
    for (fw_size_t i = 0; i < s_init_count; i++) {
        for (fw_size_t j = i + 1; j < s_init_count; j++) {
            if (s_init_entries[i].level == (uint8_t)level && s_init_entries[j].level == (uint8_t)level) {
                if (s_init_entries[i].priority > s_init_entries[j].priority) {
                    fw_init_entry_t tmp = s_init_entries[i];
                    s_init_entries[i] = s_init_entries[j];
                    s_init_entries[j] = tmp;
                }
            }
        }
    }

    /* Execute all entries belonging to this level */
    for (fw_size_t i = 0; i < s_init_count; i++) {
        if (s_init_entries[i].level == (uint8_t)level && s_init_entries[i].init_fn != FW_NULL) {
            fw_status_t status = s_init_entries[i].init_fn();
            if (status != FW_OK) {
                return status;
            }
        }
    }

    s_level_executed[level] = FW_TRUE;
    return FW_OK;
}

fw_status_t fw_init_all(void) {
    for (int lvl = 0; lvl < FW_INIT_LEVEL_COUNT; lvl++) {
        fw_status_t status = fw_init_level((fw_init_level_t)lvl);
        if (status != FW_OK) {
            return status;
        }
    }
    return FW_OK;
}

void fw_init_reset(void) {
    s_init_count = 0;
    for (int i = 0; i < FW_INIT_LEVEL_COUNT; i++) {
        s_level_executed[i] = FW_FALSE;
    }
}

fw_size_t fw_init_count(void) {
    return s_init_count;
}
