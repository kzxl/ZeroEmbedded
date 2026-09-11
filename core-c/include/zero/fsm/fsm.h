#ifndef ZERO_FSM_FSM_H
#define ZERO_FSM_FSM_H

/**
 * @file fsm.h
 * @brief Zero-Allocation Table-Driven Finite State Machine (FSM)
 * Provides deterministic, ROM-table-driven state transitions with guard conditions,
 * on-enter/on-exit state hooks, and action execution.
 */

#include "zero/types.h"
#include "zero/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t fw_fsm_state_t;
typedef uint8_t fw_fsm_event_t;

#define FW_FSM_STATE_ANY  ((fw_fsm_state_t)0xFF)
#define FW_FSM_EVENT_NONE ((fw_fsm_event_t)0x00)

typedef fw_bool_t (*fw_fsm_guard_t)(void *ctx, fw_fsm_event_t event);
typedef void (*fw_fsm_action_t)(void *ctx, fw_fsm_event_t event);
typedef void (*fw_fsm_state_hook_t)(void *ctx);

/**
 * @brief State transition descriptor placed in ROM/Flash.
 */
typedef struct {
    fw_fsm_state_t      from_state;   /*!< Origin state or FW_FSM_STATE_ANY */
    fw_fsm_event_t      event;        /*!< Triggering event */
    fw_fsm_guard_t      guard;        /*!< Optional predicate (NULL = always pass) */
    fw_fsm_action_t     action;       /*!< Optional transition action (NULL = none) */
    fw_fsm_state_t      to_state;     /*!< Destination state */
} fw_fsm_transition_t;

/**
 * @brief State configuration descriptor with enter/exit hooks.
 */
typedef struct {
    fw_fsm_state_t      state;
    fw_fsm_state_hook_t on_enter;
    fw_fsm_state_hook_t on_exit;
} fw_fsm_state_desc_t;

/**
 * @brief Finite State Machine runtime instance (stored in RAM/BSS, ~24-32 bytes).
 */
typedef struct {
    fw_fsm_state_t              current_state;
    fw_fsm_state_t              previous_state;
    const fw_fsm_transition_t  *transitions;
    fw_size_t                   transition_count;
    const fw_fsm_state_desc_t   *state_descs;
    fw_size_t                   state_desc_count;
    void                       *context;
} fw_fsm_t;

/**
 * @brief Initializes FSM instance and executes initial state's on_enter hook.
 */
fw_status_t fw_fsm_init(fw_fsm_t *fsm,
                        fw_fsm_state_t initial_state,
                        const fw_fsm_transition_t *transitions,
                        fw_size_t transition_count,
                        const fw_fsm_state_desc_t *state_descs,
                        fw_size_t state_desc_count,
                        void *context);

/**
 * @brief Dispatches an event to the state machine.
 * Evaluates matching transitions, verifies guards, runs on_exit, action, and on_enter.
 *
 * @param fsm Pointer to FSM instance.
 * @param event Event ID to process.
 * @return FW_OK if a transition was taken;
 *         FW_ERR_NOT_FOUND if no transition matched (ignored event);
 *         FW_ERR_INVALID_PARAM if fsm is NULL.
 */
fw_status_t fw_fsm_dispatch(fw_fsm_t *fsm, fw_fsm_event_t event);

/**
 * @brief Returns current state of the FSM.
 */
fw_fsm_state_t fw_fsm_get_state(const fw_fsm_t *fsm);

/**
 * @brief Returns previous state of the FSM.
 */
fw_fsm_state_t fw_fsm_get_previous_state(const fw_fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_FSM_FSM_H */
