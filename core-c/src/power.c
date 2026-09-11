#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <time.h>
#include <unistd.h>
#endif

#include "zero/hal/power.h"
#include "zero/hal/timer.h"

static uint64_t s_period_start_us = 0;
static uint64_t s_last_wake_us = 0;
static uint64_t s_total_active_us = 0;
static uint64_t s_total_sleep_us = 0;
static uint32_t s_sleep_count = 0;
static fw_bool_t s_pm_initialized = FW_FALSE;

void fw_pm_init(void) {
    uint64_t now = fw_timer_get_micros();
    s_period_start_us = now;
    s_last_wake_us = now;
    s_total_active_us = 0;
    s_total_sleep_us = 0;
    s_sleep_count = 0;
    s_pm_initialized = FW_TRUE;
}

void fw_pm_enter_sleep(uint32_t max_sleep_ms) {
    if (!s_pm_initialized) {
        fw_pm_init();
    }

    uint64_t enter_us = fw_timer_get_micros();
    uint64_t active_delta = enter_us - s_last_wake_us;
    s_total_active_us += active_delta;

    /* Execute Hardware WFI or Host Controlled Sleep */
#if defined(_WIN32) || defined(_WIN64)
    if (max_sleep_ms > 0) {
        Sleep(max_sleep_ms);
    } else {
        Sleep(1);
    }
#elif defined(__linux__) || defined(__APPLE__)
    if (max_sleep_ms > 0) {
        struct timespec ts;
        ts.tv_sec = max_sleep_ms / 1000;
        ts.tv_nsec = (max_sleep_ms % 1000) * 1000000L;
        nanosleep(&ts, FW_NULL);
    } else {
        usleep(1000);
    }
#elif defined(__arm__) || defined(__thumb__)
    (void)max_sleep_ms;
    /* ARM Cortex-M Wait For Interrupt (WFI) assembly instruction */
    __asm__ __volatile__("wfi" ::: "memory");
#else
    (void)max_sleep_ms;
    FW_COMPILER_BARRIER();
#endif

    uint64_t exit_us = fw_timer_get_micros();
    uint64_t sleep_delta = (exit_us >= enter_us) ? (exit_us - enter_us) : 0;
    s_total_sleep_us += sleep_delta;
    s_sleep_count++;
    s_last_wake_us = exit_us;
}

void fw_pm_get_metrics(fw_pm_metrics_t *out_metrics) {
    if (out_metrics == FW_NULL) return;

    /* Include current active slice up to this call */
    uint64_t now = fw_timer_get_micros();
    uint64_t current_active = s_total_active_us + (now - s_last_wake_us);
    uint64_t total_time = current_active + s_total_sleep_us;

    out_metrics->total_active_us = current_active;
    out_metrics->total_sleep_us = s_total_sleep_us;
    out_metrics->sleep_count = s_sleep_count;

    if (total_time > 0) {
        out_metrics->cpu_load_permille = (uint16_t)((current_active * 1000ULL) / total_time);
    } else {
        out_metrics->cpu_load_permille = 0;
    }
}

void fw_pm_reset_metrics(void) {
    fw_pm_init();
}
