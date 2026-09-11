/**
 * @file fsm.c
 * @brief Zero-Allocation Table-Driven Finite State Machine implementation
 */

#include "zero/fsm/fsm.h"

static const fw_fsm_state_desc_t *find_state_desc(const fw_fsm_t *fsm, fw_fsm_state_t state) {
    if (fsm->state_descs == FW_NULL || fsm->state_desc_count == 0) {
        return FW_NULL;
    }
    for (fw_size_t i = 0; i < fsm->state_desc_count; ++i) {
        if (fsm->state_descs[i].state == state) {
            return &fsm->state_descs[i];
        }
    }
    return FW_NULL;
}

fw_status_t fw_fsm_init(fw_fsm_t *fsm,
                        fw_fsm_state_t initial_state,
                        const fw_fsm_transition_t *transitions,
                        fw_size_t transition_count,
                        const fw_fsm_state_desc_t *state_descs,
                        fw_size_t state_desc_count,
                        void *context) {
    if (fsm == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    fsm->current_state      = initial_state;
    fsm->previous_state     = initial_state;
    fsm->transitions        = transitions;
    fsm->transition_count   = transition_count;
    fsm->state_descs        = state_descs;
    fsm->state_desc_count   = state_desc_count;
    fsm->context            = context;

    /* Execute entry hook for initial state */
    const fw_fsm_state_desc_t *desc = find_state_desc(fsm, initial_state);
    if (desc != FW_NULL && desc->on_enter != FW_NULL) {
        desc->on_enter(fsm->context);
    }

    return FW_OK;
}

fw_status_t fw_fsm_dispatch(fw_fsm_t *fsm, fw_fsm_event_t event) {
    if (fsm == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    if (fsm->transitions == FW_NULL || fsm->transition_count == 0) {
        return FW_ERR_NOT_FOUND;
    }

    for (fw_size_t i = 0; i < fsm->transition_count; ++i) {
        const fw_fsm_transition_t *t = &fsm->transitions[i];

        if (t->event != event) {
            continue;
        }

        if (t->from_state != fsm->current_state && t->from_state != FW_FSM_STATE_ANY) {
            continue;
        }

        if (t->guard != FW_NULL && !t->guard(fsm->context, event)) {
            continue;
        }

        /* Valid transition found -> Execute Exit -> Action -> Enter */
        fw_bool_t state_changing = (fsm->current_state != t->to_state);

        if (state_changing) {
            const fw_fsm_state_desc_t *old_desc = find_state_desc(fsm, fsm->current_state);
            if (old_desc != FW_NULL && old_desc->on_exit != FW_NULL) {
                old_desc->on_exit(fsm->context);
            }
        }

        if (t->action != FW_NULL) {
            t->action(fsm->context, event);
        }

        fsm->previous_state = fsm->current_state;
        fsm->current_state  = t->to_state;

        if (state_changing) {
            const fw_fsm_state_desc_t *new_desc = find_state_desc(fsm, fsm->current_state);
            if (new_desc != FW_NULL && new_desc->on_enter != FW_NULL) {
                new_desc->on_enter(fsm->context);
            }
        }

        return FW_OK;
    }

    return FW_ERR_NOT_FOUND;
}

fw_fsm_state_t fw_fsm_get_state(const fw_fsm_t *fsm) {
    return (fsm != FW_NULL) ? fsm->current_state : FW_FSM_STATE_ANY;
}

fw_fsm_state_t fw_fsm_get_previous_state(const fw_fsm_t *fsm) {
    return (fsm != FW_NULL) ? fsm->previous_state : FW_FSM_STATE_ANY;
}
