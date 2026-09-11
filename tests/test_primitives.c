/**
 * @file test_primitives.c
 * @brief Unit tests for Modern Embedded Primitives: Watchdog, FSM, and DSP Filters
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zero/zero.h"

/* ========================================================================== */
/* 1. Watchdog Supervisor Tests                                               */
/* ========================================================================== */

static uint32_t s_hw_kicks = 0;
static uint32_t s_fault_calls = 0;
static uint32_t s_last_starving = 0;

static void mock_hw_kick(void) {
    s_hw_kicks++;
}

static void mock_fault_handler(uint32_t starving_mask) {
    s_fault_calls++;
    s_last_starving = starving_mask;
}

static void test_watchdog_supervisor(void) {
    printf("[TEST] Watchdog Supervisor: Multi-task liveness verification...\n");

    s_hw_kicks = 0;
    s_fault_calls = 0;
    s_last_starving = 0;

    fw_wdt_supervisor_t wdt;
    fw_status_t st = fw_wdt_init(&wdt, 50, mock_hw_kick, mock_fault_handler);
    assert(st == FW_OK);

    /* Register 3 tasks: Task 0 (Comm), Task 1 (Sensors), Task 2 (Motor) */
    assert(fw_wdt_register_task(&wdt, 0) == FW_OK);
    assert(fw_wdt_register_task(&wdt, 1) == FW_OK);
    assert(fw_wdt_register_task(&wdt, 2) == FW_OK);
    assert(wdt.registered_mask == 0x07);

    /* All 3 received initial grace heartbeat -> First service should kick */
    fw_bool_t ok = fw_wdt_service(&wdt);
    assert(ok == FW_TRUE);
    assert(s_hw_kicks == 1);
    assert(wdt.reported_mask == 0); /* Window reset */

    /* Now only task 0 and task 1 report */
    fw_wdt_heartbeat(&wdt, 0);
    fw_wdt_heartbeat(&wdt, 1);

    uint32_t starving = fw_wdt_get_starving_tasks(&wdt);
    assert(starving == (1U << 2)); /* Task 2 has not reported */

    /* Service within deadline: still waiting, hw kick must NOT be called */
    ok = fw_wdt_service(&wdt);
    assert(ok == FW_TRUE);
    assert(s_hw_kicks == 1);

    /* Task 2 finally reports */
    fw_wdt_heartbeat(&wdt, 2);
    assert(fw_wdt_get_starving_tasks(&wdt) == 0);

    /* Service now triggers HW kick */
    ok = fw_wdt_service(&wdt);
    assert(ok == FW_TRUE);
    assert(s_hw_kicks == 2);

    /* Test timeout failure: task 0 reports, tasks 1 and 2 starve beyond 50ms */
    fw_wdt_heartbeat(&wdt, 0);
    fw_delay_millis(60);

    ok = fw_wdt_service(&wdt);
    assert(ok == FW_FALSE);
    assert(s_fault_calls == 1);
    assert(s_last_starving == ((1U << 1) | (1U << 2)));

    /* Test unregistering a task */
    assert(fw_wdt_unregister_task(&wdt, 2) == FW_OK);
    assert(wdt.registered_mask == 0x03);

    printf("  -> Watchdog supervisor passed! (HW Kicks: %u, Faults Caught: %u)\n",
        s_hw_kicks, s_fault_calls);
}

/* ========================================================================== */
/* 2. Table-Driven FSM Tests                                                  */
/* ========================================================================== */

enum {
    STATE_IDLE = 0,
    STATE_CONNECTING,
    STATE_ONLINE,
    STATE_ERROR
};

enum {
    EVT_CONNECT = 1,
    EVT_SUCCESS,
    EVT_FAIL,
    EVT_RESET
};

static int s_enter_idle_count = 0;
static int s_exit_idle_count = 0;
static int s_enter_online_count = 0;
static int s_action_count = 0;

static void on_enter_idle(void *ctx) { (void)ctx; s_enter_idle_count++; }
static void on_exit_idle(void *ctx)  { (void)ctx; s_exit_idle_count++; }
static void on_enter_online(void *ctx) { (void)ctx; s_enter_online_count++; }
static void on_action_connect(void *ctx, fw_fsm_event_t evt) { (void)ctx; (void)evt; s_action_count++; }

static fw_bool_t guard_allow_connect(void *ctx, fw_fsm_event_t evt) {
    (void)evt;
    int *allow_flag = (int*)ctx;
    return (*allow_flag == 1) ? FW_TRUE : FW_FALSE;
}

static const fw_fsm_state_desc_t s_fsm_states[] = {
    { STATE_IDLE,       on_enter_idle, on_exit_idle },
    { STATE_ONLINE,     on_enter_online, FW_NULL }
};

static const fw_fsm_transition_t s_fsm_transitions[] = {
    { STATE_IDLE,       EVT_CONNECT, guard_allow_connect, on_action_connect, STATE_CONNECTING },
    { STATE_CONNECTING, EVT_SUCCESS, FW_NULL,             FW_NULL,           STATE_ONLINE     },
    { STATE_CONNECTING, EVT_FAIL,    FW_NULL,             FW_NULL,           STATE_ERROR      },
    { FW_FSM_STATE_ANY, EVT_RESET,   FW_NULL,             FW_NULL,           STATE_IDLE       }
};

static void test_table_driven_fsm(void) {
    printf("[TEST] Table-Driven FSM: State transitions, guards, and hooks...\n");

    s_enter_idle_count = 0;
    s_exit_idle_count = 0;
    s_enter_online_count = 0;
    s_action_count = 0;

    int allow_flag = 0; /* Guard initially blocks transition */
    fw_fsm_t fsm;

    fw_status_t st = fw_fsm_init(&fsm, STATE_IDLE,
                                 s_fsm_transitions, sizeof(s_fsm_transitions)/sizeof(s_fsm_transitions[0]),
                                 s_fsm_states, sizeof(s_fsm_states)/sizeof(s_fsm_states[0]),
                                 &allow_flag);
    assert(st == FW_OK);
    assert(s_enter_idle_count == 1); /* Initial state entry hook called */
    assert(fw_fsm_get_state(&fsm) == STATE_IDLE);

    /* 1. Try EVT_CONNECT with guard blocking (allow_flag = 0) */
    st = fw_fsm_dispatch(&fsm, EVT_CONNECT);
    assert(st == FW_ERR_NOT_FOUND); /* Guard failed -> no transition */
    assert(fw_fsm_get_state(&fsm) == STATE_IDLE);
    assert(s_exit_idle_count == 0);
    assert(s_action_count == 0);

    /* 2. Enable guard and dispatch EVT_CONNECT */
    allow_flag = 1;
    st = fw_fsm_dispatch(&fsm, EVT_CONNECT);
    assert(st == FW_OK);
    assert(fw_fsm_get_state(&fsm) == STATE_CONNECTING);
    assert(s_exit_idle_count == 1);
    assert(s_action_count == 1);

    /* 3. Dispatch EVT_SUCCESS -> transitions to STATE_ONLINE */
    st = fw_fsm_dispatch(&fsm, EVT_SUCCESS);
    assert(st == FW_OK);
    assert(fw_fsm_get_state(&fsm) == STATE_ONLINE);
    assert(s_enter_online_count == 1);

    /* 4. Any-state transition: EVT_RESET from STATE_ONLINE back to STATE_IDLE */
    st = fw_fsm_dispatch(&fsm, EVT_RESET);
    assert(st == FW_OK);
    assert(fw_fsm_get_state(&fsm) == STATE_IDLE);
    assert(s_enter_idle_count == 2);

    printf("  -> Table-driven FSM passed! (State cycles verified with hooks)\n");
}

/* ========================================================================== */
/* 3. DSP Sensor Filters & Debounce Tests                                     */
/* ========================================================================== */

static void test_dsp_filters_and_debounce(void) {
    printf("[TEST] DSP Primitives: Fixed-Point EMA, Median-3/5, and Debounce...\n");

    /* 1. EMA Filter Test */
    uint16_t ema = 1000;
    /* alpha_shift = 1 (50% smoothing): (1000 + (2000 - 1000)/2) = 1500 */
    ema = fw_filter_ema_u16(ema, 2000, 1);
    assert(ema == 1500);

    /* Steady input remains unchanged */
    ema = fw_filter_ema_u16(ema, 1500, 2);
    assert(ema == 1500);

    /* 2. Median-3 Filter (Spike Rejection) */
    assert(fw_filter_median3_u16(100, 5000, 105) == 105);
    assert(fw_filter_median3_u16(5000, 100, 105) == 105);
    assert(fw_filter_median3_u16(105, 100, 5000) == 105);

    /* 3. Median-5 Filter (Complex Spike Rejection) */
    assert(fw_filter_median5_u16(10, 5000, 12, 11, 14) == 12);
    assert(fw_filter_median5_u16(1, 2, 3, 4, 5) == 3);
    assert(fw_filter_median5_u16(5, 4, 3, 2, 1) == 3);
    assert(fw_filter_median5_u16(99, 10, 80, 20, 50) == 50);

    /* 4. Debounce Filter */
    fw_debounce_t btn;
    fw_debounce_init(&btn, 3, FW_FALSE); /* Requires 3 identical samples */
    assert(fw_debounce_is_active(&btn) == FW_FALSE);

    /* Sample 1: HIGH */
    fw_bool_t toggled = fw_debounce_update(&btn, FW_TRUE);
    assert(toggled == FW_FALSE);
    assert(fw_debounce_is_active(&btn) == FW_FALSE);

    /* Glitch: LOW (should reset counter) */
    toggled = fw_debounce_update(&btn, FW_FALSE);
    assert(toggled == FW_FALSE);
    assert(fw_debounce_is_active(&btn) == FW_FALSE);

    /* 3 consecutive HIGH samples */
    assert(fw_debounce_update(&btn, FW_TRUE) == FW_FALSE);
    assert(fw_debounce_update(&btn, FW_TRUE) == FW_FALSE);
    toggled = fw_debounce_update(&btn, FW_TRUE); /* 3rd sample triggers transition! */
    assert(toggled == FW_TRUE);
    assert(fw_debounce_is_active(&btn) == FW_TRUE);

    printf("  -> DSP Filters & Debounce passed! (EMA, Median3/5, and Debounce verified)\n");
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void) {
    printf("\n====================================================================================\n");
    printf("   ZEROEMBEDDED MODERN PRIMITIVES TEST SUITE (Watchdog, FSM, DSP Filters)\n");
    printf("====================================================================================\n\n");

    test_watchdog_supervisor();
    test_table_driven_fsm();
    test_dsp_filters_and_debounce();

    printf("\n\033[1;32m[PASS] ALL MODERN EMBEDDED PRIMITIVES TESTS PASSED SUCCESSFULLY!\033[0m\n\n");
    return 0;
}
