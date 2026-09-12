#ifndef ZERO_SYNC_HOOK_H
#define ZERO_SYNC_HOOK_H

/**
 * @file hook.h
 * @brief Priority-ordered Non-intrusive Event Hook (CBR) System.
 * Inspired by Magic Lantern's CallBack Routine (ml-cbr) interception framework.
 */

#include "../types.h"
#include "../result.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FW_HOOK_STOP     = 0, /**< Intercept event and halt further dispatch */
    FW_HOOK_CONTINUE = 1  /**< Allow lower priority hooks to process */
} fw_hook_action_t;

typedef fw_hook_action_t (*fw_hook_fn_t)(uint32_t event_id, void *event_data, void *cookie);

typedef struct {
    uint32_t      event_id;
    fw_hook_fn_t  callback;
    void         *cookie;
    uint8_t       priority; /**< Lower number = higher execution priority */
    fw_bool_t     active;
} fw_hook_entry_t;

#define FW_HOOK_MAX_ENTRIES 32

typedef struct {
    fw_hook_entry_t entries[FW_HOOK_MAX_ENTRIES];
    fw_size_t       count;
    fw_size_t       total_dispatches;
    fw_size_t       total_intercepts;
} fw_hook_table_t;

/**
 * @brief Initializes a priority hook table.
 */
fw_status_t fw_hook_table_init(fw_hook_table_t *table);

/**
 * @brief Registers a hook callback for an event with priority.
 *
 * @param table Pointer to hook table.
 * @param event_id User-defined event identifier.
 * @param callback Callback function.
 * @param priority Execution priority (0 = highest).
 * @param cookie User-defined context pointer passed to callback.
 */
fw_status_t fw_hook_register(
    fw_hook_table_t *table,
    uint32_t         event_id,
    fw_hook_fn_t     callback,
    uint8_t          priority,
    void            *cookie
);

/**
 * @brief Unregisters a previously registered callback.
 */
fw_status_t fw_hook_unregister(
    fw_hook_table_t *table,
    uint32_t         event_id,
    fw_hook_fn_t     callback
);

/**
 * @brief Dispatches an event through registered hooks in descending priority order.
 * If any hook returns FW_HOOK_STOP, execution ceases immediately.
 *
 * @return FW_HOOK_CONTINUE if all handlers ran, FW_HOOK_STOP if intercepted.
 */
fw_hook_action_t fw_hook_dispatch(
    fw_hook_table_t *table,
    uint32_t         event_id,
    void            *event_data
);

/**
 * @brief Returns the count of active hooks registered for an event.
 */
fw_size_t fw_hook_count(const fw_hook_table_t *table, uint32_t event_id);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_SYNC_HOOK_H */
