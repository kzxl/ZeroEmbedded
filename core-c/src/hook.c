#include "zero/sync/hook.h"
#include <string.h>

fw_status_t fw_hook_table_init(fw_hook_table_t *table) {
    if (table == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }
    memset(table, 0, sizeof(*table));
    return FW_OK;
}

fw_status_t fw_hook_register(
    fw_hook_table_t *table,
    uint32_t         event_id,
    fw_hook_fn_t     callback,
    uint8_t          priority,
    void            *cookie
) {
    if (table == FW_NULL || callback == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    /* Check if already registered or find first inactive slot */
    int free_slot = -1;
    for (fw_size_t i = 0; i < FW_HOOK_MAX_ENTRIES; i++) {
        if (table->entries[i].active) {
            if (table->entries[i].event_id == event_id && table->entries[i].callback == callback) {
                /* Update priority and cookie */
                table->entries[i].priority = priority;
                table->entries[i].cookie = cookie;
                return FW_OK;
            }
        } else if (free_slot == -1) {
            free_slot = (int)i;
        }
    }

    if (free_slot == -1) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    fw_hook_entry_t *e = &table->entries[free_slot];
    e->event_id = event_id;
    e->callback = callback;
    e->priority = priority;
    e->cookie = cookie;
    e->active = FW_TRUE;
    table->count++;

    return FW_OK;
}

fw_status_t fw_hook_unregister(
    fw_hook_table_t *table,
    uint32_t         event_id,
    fw_hook_fn_t     callback
) {
    if (table == FW_NULL || callback == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    for (fw_size_t i = 0; i < FW_HOOK_MAX_ENTRIES; i++) {
        if (table->entries[i].active &&
            table->entries[i].event_id == event_id &&
            table->entries[i].callback == callback) {
            table->entries[i].active = FW_FALSE;
            if (table->count > 0) {
                table->count--;
            }
            return FW_OK;
        }
    }

    return FW_ERR_NOT_FOUND;
}

fw_hook_action_t fw_hook_dispatch(
    fw_hook_table_t *table,
    uint32_t         event_id,
    void            *event_data
) {
    if (table == FW_NULL) {
        return FW_HOOK_CONTINUE;
    }

    table->total_dispatches++;

    /* Collect matching active entries */
    fw_hook_entry_t *matched[FW_HOOK_MAX_ENTRIES];
    fw_size_t match_count = 0;

    for (fw_size_t i = 0; i < FW_HOOK_MAX_ENTRIES; i++) {
        if (table->entries[i].active && table->entries[i].event_id == event_id) {
            matched[match_count++] = &table->entries[i];
        }
    }

    if (match_count == 0) {
        return FW_HOOK_CONTINUE;
    }

    /* Sort by priority ascending (0 = highest priority) */
    for (fw_size_t i = 0; i < match_count; i++) {
        for (fw_size_t j = i + 1; j < match_count; j++) {
            if (matched[i]->priority > matched[j]->priority) {
                fw_hook_entry_t *tmp = matched[i];
                matched[i] = matched[j];
                matched[j] = tmp;
            }
        }
    }

    /* Execute in priority order */
    for (fw_size_t i = 0; i < match_count; i++) {
        fw_hook_entry_t *e = matched[i];
        if (e->callback != FW_NULL) {
            fw_hook_action_t action = e->callback(event_id, event_data, e->cookie);
            if (action == FW_HOOK_STOP) {
                table->total_intercepts++;
                return FW_HOOK_STOP;
            }
        }
    }

    return FW_HOOK_CONTINUE;
}

fw_size_t fw_hook_count(const fw_hook_table_t *table, uint32_t event_id) {
    if (table == FW_NULL) {
        return 0;
    }

    fw_size_t c = 0;
    for (fw_size_t i = 0; i < FW_HOOK_MAX_ENTRIES; i++) {
        if (table->entries[i].active && table->entries[i].event_id == event_id) {
            c++;
        }
    }
    return c;
}
