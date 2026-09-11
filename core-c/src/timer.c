#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#include <windows.h>
#endif

#include "zero/hal/timer.h"

#if defined(_WIN32) || defined(_WIN64)

static LARGE_INTEGER s_timer_freq;
static LARGE_INTEGER s_timer_start;
static int s_timer_initialized = 0;

static void init_host_timer(void) {
    if (!s_timer_initialized) {
        QueryPerformanceFrequency(&s_timer_freq);
        QueryPerformanceCounter(&s_timer_start);
        s_timer_initialized = 1;
    }
}

uint64_t fw_timer_get_micros(void) {
    init_host_timer();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    uint64_t elapsed = (uint64_t)(now.QuadPart - s_timer_start.QuadPart);
    return (elapsed * 1000000ULL) / (uint64_t)s_timer_freq.QuadPart;
}

uint32_t fw_timer_get_millis(void) {
    return (uint32_t)(fw_timer_get_micros() / 1000ULL);
}

void fw_delay_micros(uint32_t us) {
    uint64_t start = fw_timer_get_micros();
    while ((fw_timer_get_micros() - start) < (uint64_t)us) {
        /* Busy wait */
    }
}

void fw_delay_millis(uint32_t ms) {
    uint32_t start = fw_timer_get_millis();
    while ((fw_timer_get_millis() - start) < ms) {
        /* Busy wait */
    }
}

#elif defined(__linux__) || defined(__APPLE__)
#include <time.h>
#include <unistd.h>

static struct timespec s_start_time;
static int s_timer_initialized = 0;

static void init_host_timer(void) {
    if (!s_timer_initialized) {
        clock_gettime(CLOCK_MONOTONIC, &s_start_time);
        s_timer_initialized = 1;
    }
}

uint64_t fw_timer_get_micros(void) {
    init_host_timer();
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t sec = (uint64_t)(now.tv_sec - s_start_time.tv_sec);
    int64_t nsec = now.tv_nsec - s_start_time.tv_nsec;
    return (sec * 1000000ULL) + (uint64_t)(nsec / 1000L);
}

uint32_t fw_timer_get_millis(void) {
    return (uint32_t)(fw_timer_get_micros() / 1000ULL);
}

void fw_delay_micros(uint32_t us) {
    uint64_t start = fw_timer_get_micros();
    while ((fw_timer_get_micros() - start) < (uint64_t)us) {
        /* Busy wait */
    }
}

void fw_delay_millis(uint32_t ms) {
    uint32_t start = fw_timer_get_millis();
    while ((fw_timer_get_millis() - start) < ms) {
        /* Busy wait */
    }
}

#else
/* Bare-metal / Embedded Target Implementation */
static volatile uint32_t s_embedded_millis = 0;

void fw_timer_systick_tick(void) {
    s_embedded_millis++;
}

uint32_t fw_timer_get_millis(void) {
    return s_embedded_millis;
}

uint64_t fw_timer_get_micros(void) {
    /* Fallback approximation if DWT cycle counter is unavailable */
    return (uint64_t)s_embedded_millis * 1000ULL;
}

void fw_delay_micros(uint32_t us) {
    /* Software calibrated spinloop fallback (e.g. ~16 cycles per loop @ 64MHz) */
    volatile uint32_t count = us * 8;
    while (count--) {
        FW_COMPILER_BARRIER();
    }
}

void fw_delay_millis(uint32_t ms) {
    uint32_t start = fw_timer_get_millis();
    while ((fw_timer_get_millis() - start) < ms) {
        FW_COMPILER_BARRIER();
    }
}
#endif
