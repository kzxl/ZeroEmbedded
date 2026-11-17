#include "zero/sync/hook.h"
#include <string.h>

fw_status_t fw_hook_table_init(fw_hook_table_t *table) {
    if (table == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }
    memset(table, 0, sizeof(*table));
    return FW_OK;
}

static void hook_remove_at(fw_hook_table_t *table, fw_size_t idx) {
    for (fw_size_t j = idx; j + 1U < table->count; ++j) {
        table->entries[j] = table->entries[j + 1U];
    }
    table->count--;
    memset(&table->entries[table->count], 0, sizeof(fw_hook_entry_t));
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

    for (fw_size_t i = 0; i < table->count; i++) {
        if (table->entries[i].active &&
            table->entries[i].event_id == event_id &&
            table->entries[i].callback == callback) {
            if (table->entries[i].priority == priority) {
                table->entries[i].cookie = cookie;
                return FW_OK;
            }
            hook_remove_at(table, i);
            break;
        }
    }

    if (table->count >= FW_HOOK_MAX_ENTRIES) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    /* Insert sorted by priority ascending (0 = highest priority) */
    fw_size_t ins = table->count;
    while (ins > 0U && table->entries[ins - 1U].priority > priority) {
        table->entries[ins] = table->entries[ins - 1U];
        ins--;
    }

    fw_hook_entry_t *e = &table->entries[ins];
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

    for (fw_size_t i = 0; i < table->count; i++) {
        if (table->entries[i].active &&
            table->entries[i].event_id == event_id &&
            table->entries[i].callback == callback) {
            hook_remove_at(table, i);
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

    /* Table is kept sorted by priority at registration; zero stack overhead here */
    for (fw_size_t i = 0; i < table->count; i++) {
        fw_hook_entry_t *e = &table->entries[i];
        if (e->active && e->event_id == event_id && e->callback != FW_NULL) {
            if (e->callback(event_id, event_data, e->cookie) == FW_HOOK_STOP) {
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
    for (fw_size_t i = 0; i < table->count; i++) {
        if (table->entries[i].active && table->entries[i].event_id == event_id) {
            c++;
        }
    }
    return c;
}
