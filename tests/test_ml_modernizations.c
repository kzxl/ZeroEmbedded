#include <stdio.h>
#include <string.h>
#include "zero/zero.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        g_tests_run++; \
        if (cond) { \
            g_tests_passed++; \
        } else { \
            printf("[FAIL] Line %d: %s\n", __LINE__, msg); \
            return FW_ERR_GENERIC; \
        } \
    } while(0)

/* ========================================================================== */
/* 1. Test Linker Table Auto-Registration Engine                              */
/* ========================================================================== */
static int s_init_execution_log[8];
static int s_init_log_idx = 0;

static fw_status_t init_early_prio0(void) {
    s_init_execution_log[s_init_log_idx++] = 10;
    return FW_OK;
}
static fw_status_t init_core_prio10(void) {
    s_init_execution_log[s_init_log_idx++] = 20;
    return FW_OK;
}
static fw_status_t init_core_prio5(void) {
    s_init_execution_log[s_init_log_idx++] = 25;
    return FW_OK;
}
static fw_status_t init_app_prio0(void) {
    s_init_execution_log[s_init_log_idx++] = 30;
    return FW_OK;
}

static fw_status_t test_init_engine(void) {
    fw_init_reset();
    s_init_log_idx = 0;

    TEST_ASSERT(fw_init_register("early", init_early_prio0, FW_INIT_LEVEL_EARLY, 0) == FW_OK, "Register early");
    TEST_ASSERT(fw_init_register("core10", init_core_prio10, FW_INIT_LEVEL_CORE, 10) == FW_OK, "Register core10");
    TEST_ASSERT(fw_init_register("core5", init_core_prio5, FW_INIT_LEVEL_CORE, 5) == FW_OK, "Register core5");
    TEST_ASSERT(fw_init_register("app", init_app_prio0, FW_INIT_LEVEL_APP, 0) == FW_OK, "Register app");

    TEST_ASSERT(fw_init_count() == 4, "Should have 4 registered entries");

    /* Execute all levels */
    TEST_ASSERT(fw_init_all() == FW_OK, "Init all must succeed");
    TEST_ASSERT(s_init_log_idx == 4, "All 4 functions must have run");

    /* Verify order: Early(10), Core5(25 - lower prio val runs first), Core10(20), App(30) */
    TEST_ASSERT(s_init_execution_log[0] == 10, "Early must run first");
    TEST_ASSERT(s_init_execution_log[1] == 25, "Core prio 5 runs before core prio 10");
    TEST_ASSERT(s_init_execution_log[2] == 20, "Core prio 10 runs after core prio 5");
    TEST_ASSERT(s_init_execution_log[3] == 30, "App runs last");

    return FW_OK;
}

/* ========================================================================== */
/* 2. Test Multi-Tier Memory Hub & Safety Governor                            */
/* ========================================================================== */
static fw_status_t test_memory_hub(void) {
    fw_mem_hub_t hub;
    TEST_ASSERT(fw_mem_hub_init(&hub) == FW_OK, "Hub init must succeed");

    /* Setup Pool for FAST tier: 4 blocks of 64 bytes (288 bytes buffer includes bitmap) */
    FW_ALIGNED(8) static uint8_t pool_mem[288];
    fw_pool_t pool;
    TEST_ASSERT(fw_pool_init(&pool, pool_mem, sizeof(pool_mem), 64, 8) == FW_OK, "Pool init");
    TEST_ASSERT(pool.capacity == 4, "Pool capacity must be exactly 4 blocks");

    /* Reserve 64 bytes in FAST tier (1 block safety margin) */
    TEST_ASSERT(fw_mem_hub_register_pool(&hub, FW_MEM_TIER_FAST, &pool, "fast_sram", 64) == FW_OK, "Register pool");

    /* Setup Arena for BULK tier: 512 bytes */
    FW_ALIGNED(8) static uint8_t arena_mem[512];
    fw_arena_t arena;
    TEST_ASSERT(fw_arena_init(&arena, arena_mem, sizeof(arena_mem)) == FW_OK, "Arena init");

    /* Reserve 128 bytes in BULK tier */
    TEST_ASSERT(fw_mem_hub_register_arena(&hub, FW_MEM_TIER_BULK, &arena, "bulk_sdram", 128) == FW_OK, "Register arena");

    /* Allocate from FAST tier */
    void *p1 = fw_mem_hub_alloc(&hub, 48, FW_MEM_HINT_FAST);
    TEST_ASSERT(p1 != FW_NULL, "Alloc from FAST tier must succeed");
    TEST_ASSERT(p1 >= (void*)pool_mem && p1 < (void*)(pool_mem + sizeof(pool_mem)), "p1 must belong to pool");

    /* Allocate from BULK tier */
    void *p2 = fw_mem_hub_alloc(&hub, 100, FW_MEM_HINT_BULK);
    TEST_ASSERT(p2 != FW_NULL, "Alloc from BULK tier must succeed");
    TEST_ASSERT(p2 >= (void*)arena_mem && p2 < (void*)(arena_mem + sizeof(arena_mem)), "p2 must belong to arena");

    /* Verify safety margin enforcement:
     * Pool capacity = 4 blocks. 1 block used (p1), 3 remaining.
     * Allocating 2 more blocks leaves 1 block (64 bytes), which matches reserve_bytes (64).
     * Allocating a 4th block must be REJECTED to preserve reserve_bytes!
     */
    void *p3 = fw_mem_hub_alloc(&hub, 32, FW_MEM_HINT_FAST);
    void *p4 = fw_mem_hub_alloc(&hub, 32, FW_MEM_HINT_FAST);
    TEST_ASSERT(p3 != FW_NULL && p4 != FW_NULL, "p3 and p4 should succeed");

    void *p5 = fw_mem_hub_alloc(&hub, 32, FW_MEM_HINT_FAST);
    /* Since reserve_bytes is 64, this allocation would breach margin -> should fallback or reject */
    /* If fallback allowed, it goes to BULK arena */
    if (p5 != FW_NULL) {
        TEST_ASSERT(p5 >= (void*)arena_mem && p5 < (void*)(arena_mem + sizeof(arena_mem)), "p5 must fallback to BULK");
    }

    /* Free back p1 to pool via hub */
    TEST_ASSERT(fw_mem_hub_free(&hub, p1) == FW_OK, "Free p1 via hub");
    TEST_ASSERT(fw_mem_hub_free(&hub, p3) == FW_OK, "Free p3 via hub");
    TEST_ASSERT(fw_mem_hub_free(&hub, p4) == FW_OK, "Free p4 via hub");

    return FW_OK;
}

/* ========================================================================== */
/* 3. Test Priority Event Hook Engine (ml-cbr pattern)                        */
/* ========================================================================== */
static int s_hook_trace[8];
static int s_hook_trace_idx = 0;

static fw_hook_action_t hook_normal_low(uint32_t event_id, void *data, void *cookie) {
    (void)event_id; (void)data; (void)cookie;
    s_hook_trace[s_hook_trace_idx++] = 100;
    return FW_HOOK_CONTINUE;
}

static fw_hook_action_t hook_normal_high(uint32_t event_id, void *data, void *cookie) {
    (void)event_id; (void)data; (void)cookie;
    s_hook_trace[s_hook_trace_idx++] = 10;
    return FW_HOOK_CONTINUE;
}

static fw_hook_action_t hook_blocker(uint32_t event_id, void *data, void *cookie) {
    (void)event_id; (void)data; (void)cookie;
    s_hook_trace[s_hook_trace_idx++] = 1;
    return FW_HOOK_STOP; /* Intercept */
}

static fw_status_t test_priority_hook(void) {
    fw_hook_table_t table;
    TEST_ASSERT(fw_hook_table_init(&table) == FW_OK, "Hook table init");

    uint32_t EVT_SHUTTER = 0x42;

    /* Register low priority (prio 50) and high priority (prio 10) */
    TEST_ASSERT(fw_hook_register(&table, EVT_SHUTTER, hook_normal_low, 50, FW_NULL) == FW_OK, "Reg low");
    TEST_ASSERT(fw_hook_register(&table, EVT_SHUTTER, hook_normal_high, 10, FW_NULL) == FW_OK, "Reg high");

    s_hook_trace_idx = 0;
    fw_hook_action_t act = fw_hook_dispatch(&table, EVT_SHUTTER, FW_NULL);
    TEST_ASSERT(act == FW_HOOK_CONTINUE, "Should continue");
    TEST_ASSERT(s_hook_trace_idx == 2, "Both hooks should have run");
    TEST_ASSERT(s_hook_trace[0] == 10 && s_hook_trace[1] == 100, "High priority (10) must run before low (100)");

    /* Now register interceptor with highest priority (prio 0) */
    TEST_ASSERT(fw_hook_register(&table, EVT_SHUTTER, hook_blocker, 0, FW_NULL) == FW_OK, "Reg blocker");
    s_hook_trace_idx = 0;

    act = fw_hook_dispatch(&table, EVT_SHUTTER, FW_NULL);
    TEST_ASSERT(act == FW_HOOK_STOP, "Blocker must stop propagation");
    TEST_ASSERT(s_hook_trace_idx == 1, "Only blocker should have run");
    TEST_ASSERT(s_hook_trace[0] == 1, "Blocker executed");

    /* Unregister blocker and dispatch again */
    TEST_ASSERT(fw_hook_unregister(&table, EVT_SHUTTER, hook_blocker) == FW_OK, "Unreg blocker");
    s_hook_trace_idx = 0;
    act = fw_hook_dispatch(&table, EVT_SHUTTER, FW_NULL);
    TEST_ASSERT(act == FW_HOOK_CONTINUE, "Should continue after blocker removed");
    TEST_ASSERT(s_hook_trace_idx == 2, "Both original hooks ran again");

    return FW_OK;
}

/* ========================================================================== */
/* 4. Test DMA Memory Coprocessor & 2D Strided Transfer                       */
/* ========================================================================== */
static fw_status_t test_dma_and_2d_copy(void) {
    fw_dma_reset_stats();

    /* 1D Memcpy test */
    uint32_t src[16];
    uint32_t dst[16];
    for (int i = 0; i < 16; i++) {
        src[i] = (uint32_t)(0xAA00 + i);
    }
    memset(dst, 0, sizeof(dst));

    TEST_ASSERT(fw_dma_memcpy(dst, src, sizeof(src)) == FW_OK, "DMA memcpy");
    TEST_ASSERT(memcmp(src, dst, sizeof(src)) == 0, "DMA memcpy data verify");

    /* 1D Memset test */
    TEST_ASSERT(fw_dma_memset(dst, 0x55, sizeof(dst)) == FW_OK, "DMA memset");
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT(dst[i] == 0x55555555U, "DMA memset data verify");
    }

    /* 2D Strided Transfer Test (4x4 matrix, extract center 2x2 into 2x2 dst) */
    /*
       Source 4x4 matrix (stride = 4 bytes):
       01 02 03 04
       05[06 07]08
       09[10 11]12
       13 14 15 16
    */
    uint8_t src_grid[4][4] = {
        { 1,  2,  3,  4},
        { 5,  6,  7,  8},
        { 9, 10, 11, 12},
        {13, 14, 15, 16}
    };

    uint8_t dst_grid[2][2];
    memset(dst_grid, 0, sizeof(dst_grid));

    /* Source starts at src_grid[1][1] = value 6. Pitch is 4 bytes.
     * Destination is 2x2, pitch is 2 bytes. Width = 2 bytes, Height = 2 lines.
     */
    TEST_ASSERT(fw_dma_copy_2d(&dst_grid[0][0], 2, &src_grid[1][1], 4, 2, 2) == FW_OK, "2D DMA copy");

    TEST_ASSERT(dst_grid[0][0] == 6, "Pixel [0,0] == 6");
    TEST_ASSERT(dst_grid[0][1] == 7, "Pixel [0,1] == 7");
    TEST_ASSERT(dst_grid[1][0] == 10, "Pixel [1,0] == 10");
    TEST_ASSERT(dst_grid[1][1] == 11, "Pixel [1,1] == 11");

    fw_dma_stats_t stats;
    fw_dma_get_stats(&stats);
    TEST_ASSERT(stats.total_transfers == 3, "Total transfers should be 3");
    TEST_ASSERT(stats.total_2d_transfers == 1, "Total 2D transfers should be 1");

    return FW_OK;
}

/* ========================================================================== */
/* 5. Test Task Stack Watermark Profiler                                      */
/* ========================================================================== */
static fw_status_t test_stack_monitor(void) {
    FW_ALIGNED(4) static uint32_t dummy_stack[128]; /* 512 bytes */
    fw_size_t stack_size = sizeof(dummy_stack);

    TEST_ASSERT(fw_task_stack_paint(dummy_stack, stack_size) == FW_OK, "Stack paint");

    /* Unused should be 100% initially */
    TEST_ASSERT(fw_task_stack_unused(dummy_stack, stack_size) == stack_size, "Initially 100% unused");
    TEST_ASSERT(fw_task_stack_peak(dummy_stack, stack_size) == 0, "Initial peak is 0");

    /* Simulate stack usage: corrupt the top 32 words (128 bytes) */
    /* Stack grows downwards, so top of memory is dummy_stack[96..127] */
    for (int i = 96; i < 128; i++) {
        dummy_stack[i] = 0x12345678U;
    }

    fw_size_t peak = fw_task_stack_peak(dummy_stack, stack_size);
    TEST_ASSERT(peak == (32 * sizeof(uint32_t)), "Peak must measure 128 bytes");
    TEST_ASSERT(fw_task_stack_unused(dummy_stack, stack_size) == (stack_size - 128), "Unused must be 384 bytes");

    /* Record in task monitor */
    fw_task_monitor_reset();
    TEST_ASSERT(fw_task_monitor_record(1, 450, peak) == FW_OK, "Record metric");

    fw_task_metric_t m;
    TEST_ASSERT(fw_task_monitor_get(1, &m) == FW_OK, "Get metric");
    TEST_ASSERT(m.task_id == 1, "Task ID matches");
    TEST_ASSERT(m.total_runtime_us == 450, "Runtime matches");
    TEST_ASSERT(m.stack_peak_bytes == 128, "Peak matches");

    return FW_OK;
}

/* ========================================================================== */
/* 6. Test Boot Loop Guard & Safe Mode Recovery                               */
/* ========================================================================== */
static fw_status_t test_boot_guard(void) {
    fw_boot_guard_t guard;
    memset(&guard, 0, sizeof(guard));

    /* Normal clean boot: max 3 crashes allowed */
    TEST_ASSERT(fw_boot_guard_init(&guard, 3) == FW_OK, "Boot 1 init");
    TEST_ASSERT(guard.boot_attempts == 1, "Boot attempts == 1");
    TEST_ASSERT(guard.consecutive_crashes == 1, "Crashes == 1");
    TEST_ASSERT(fw_boot_guard_is_safe_mode(&guard) == FW_FALSE, "Not safe mode yet");

    /* System stable -> mark success */
    TEST_ASSERT(fw_boot_guard_mark_success(&guard) == FW_OK, "Mark success");
    TEST_ASSERT(guard.consecutive_crashes == 0, "Crash counter cleared");

    /* Simulate crash loop: 3 crashes without mark_success */
    fw_boot_guard_init(&guard, 3); /* Crash 1 */
    TEST_ASSERT(fw_boot_guard_is_safe_mode(&guard) == FW_FALSE, "Crash 1 safe mode false");

    fw_boot_guard_init(&guard, 3); /* Crash 2 */
    TEST_ASSERT(fw_boot_guard_is_safe_mode(&guard) == FW_FALSE, "Crash 2 safe mode false");

    fw_boot_guard_init(&guard, 3); /* Crash 3 -> triggers threshold */
    TEST_ASSERT(fw_boot_guard_is_safe_mode(&guard) == FW_TRUE, "Crash 3 triggers Safe Mode!");

    /* Manual safe mode override */
    fw_boot_guard_set_safe_mode(&guard, FW_FALSE);
    TEST_ASSERT(fw_boot_guard_is_safe_mode(&guard) == FW_FALSE, "Safe mode manual clear");

    return FW_OK;
}

/* ========================================================================== */
/* 7. Test Cache Coherency API                                                */
/* ========================================================================== */
static fw_status_t test_cache_coherency(void) {
    uint8_t buffer[128];
    memset(buffer, 0xAA, sizeof(buffer));

    fw_cache_clean(buffer, sizeof(buffer));
    fw_cache_invalidate(buffer, sizeof(buffer));
    fw_cache_flush_all();

    /* Ensure buffer remains valid and accessible */
    TEST_ASSERT(buffer[0] == 0xAA, "Cache coherency memory access");

    return FW_OK;
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */
int main(void) {
    printf("===============================================================\n");
    printf(" ⚡ ZeroEmbedded: Magic Lantern Modernizations & Enhancements\n");
    printf("===============================================================\n");

    if (test_init_engine() != FW_OK) return 1;
    printf("[PASS] Linker Table Auto-Registration Engine\n");

    if (test_memory_hub() != FW_OK) return 1;
    printf("[PASS] Multi-Tier Memory Hub Router & Safety Margin\n");

    if (test_priority_hook() != FW_OK) return 1;
    printf("[PASS] Priority Event Hook Engine (ml-cbr)\n");

    if (test_dma_and_2d_copy() != FW_OK) return 1;
    printf("[PASS] DMA Memory Coprocessor & 2D Strided Transfer\n");

    if (test_stack_monitor() != FW_OK) return 1;
    printf("[PASS] Task Stack High-Water Mark Profiler (tskmon)\n");

    if (test_boot_guard() != FW_OK) return 1;
    printf("[PASS] Boot Loop Guard & Safe Mode Recovery (LOADING.LCK)\n");

    if (test_cache_coherency() != FW_OK) return 1;
    printf("[PASS] Hardware Cache Coherency & CP15 Barriers\n");

    printf("\nAll Magic Lantern modernizations passed (%d/%d assertions)!\n",
           g_tests_passed, g_tests_run);
    return 0;
}
