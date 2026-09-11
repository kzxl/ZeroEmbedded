#include <stdio.h>
#include <string.h>
#include "zero/zero.h"
#include "zero/protocol/zerowire.h"

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
/* Test 1: Primitive Types and Static Asserts                                 */
/* ========================================================================== */
static fw_status_t test_primitives(void) {
    FW_STATIC_ASSERT(sizeof(fw_u8_t) == 1, "fw_u8_t must be 1 byte");
    FW_STATIC_ASSERT(sizeof(fw_u16_t) == 2, "fw_u16_t must be 2 bytes");
    FW_STATIC_ASSERT(sizeof(fw_u32_t) == 4, "fw_u32_t must be 4 bytes");
    FW_STATIC_ASSERT(sizeof(fw_u64_t) == 8, "fw_u64_t must be 8 bytes");
    FW_STATIC_ASSERT(sizeof(fw_span_t) == 2 * sizeof(void*), "fw_span_t must be 2 words");

    TEST_ASSERT(fw_is_ok(FW_OK), "FW_OK must be ok");
    TEST_ASSERT(fw_is_err(FW_ERR_GENERIC), "FW_ERR_GENERIC must be err");

    return FW_OK;
}

/* ========================================================================== */
/* Test 2: Span and String View                                               */
/* ========================================================================== */
static fw_status_t test_span_and_string_view(void) {
    uint8_t buffer[64];
    for (size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = (uint8_t)(i + 1);
    }

    fw_span_t span = FW_SPAN_FROM_ARRAY(buffer);
    TEST_ASSERT(span.length == 64, "Span length must match array size");
    TEST_ASSERT(!fw_span_is_empty(span), "Span should not be empty");

    fw_span_t sub = fw_span_sub(span, 10, 20);
    TEST_ASSERT(sub.length == 20, "Subspan length must be 20");
    TEST_ASSERT(*((uint8_t*)sub.data) == 11, "Subspan offset 0 should match buffer[10]");

    /* Test fw_span_sub_safe bounds check */
    fw_span_t safe_sub;
    TEST_ASSERT(fw_span_sub_safe(span, 10, 1000, &safe_sub) == FW_ERR_BUFFER_OVERFLOW, "Subspan overflow caught");
    TEST_ASSERT(safe_sub.data == FW_NULL, "Safe subspan set to null on overflow");

    fw_string_view_t sv1 = FW_SV_LITERAL("ZeroEmbedded");
    fw_string_view_t sv2 = fw_sv_from_cstr("ZeroEmbedded");
    fw_string_view_t sv3 = FW_SV_LITERAL("ZeroPlatform");

    TEST_ASSERT(fw_sv_equals(sv1, sv2), "Identical strings must equal");
    TEST_ASSERT(!fw_sv_equals(sv1, sv3), "Different strings must not equal");
    TEST_ASSERT(sv1.length == 12, "Length must be 12");

    return FW_OK;
}

/* ========================================================================== */
/* Test 3: Fixed-Size Block Memory Pool                                       */
/* ========================================================================== */
static fw_status_t test_memory_pool(void) {
    FW_ALIGNED(16) static uint8_t raw_memory[512];
    fw_pool_t pool;

    /* 512 bytes with 32-byte blocks -> sufficient for bitmap + blocks */
    fw_status_t status = fw_pool_init(&pool, raw_memory, sizeof(raw_memory), 32, 8);
    TEST_ASSERT(status == FW_OK, "Pool init must succeed");
    fw_size_t cap = fw_pool_capacity(&pool);
    TEST_ASSERT(cap > 0, "Capacity should be > 0");
    TEST_ASSERT(fw_pool_available(&pool) == cap, "Available should match capacity");

    void *blocks[32];
    for (fw_size_t i = 0; i < cap; ++i) {
        blocks[i] = fw_pool_alloc(&pool);
        TEST_ASSERT(blocks[i] != FW_NULL, "Allocation must succeed");
    }

    TEST_ASSERT(fw_pool_is_exhausted(&pool), "Pool should be exhausted");
    TEST_ASSERT(fw_pool_alloc(&pool) == FW_NULL, "Alloc on exhausted pool must return NULL");

    /* Free one block and re-allocate */
    TEST_ASSERT(fw_pool_free(&pool, blocks[0]) == FW_OK, "Free must succeed");
    TEST_ASSERT(fw_pool_available(&pool) == 1, "Available count must be 1");

    /* CRITICAL SAFETY TEST: DOUBLE-FREE REJECTION */
    TEST_ASSERT(fw_pool_free(&pool, blocks[0]) == FW_ERR_INVALID_ARG, "DOUBLE-FREE MUST BE REJECTED");

    void *reallocated = fw_pool_alloc(&pool);
    TEST_ASSERT(reallocated == blocks[0], "Should recycle recently freed block");
    TEST_ASSERT(fw_pool_is_exhausted(&pool), "Pool should be exhausted again");

    /* Free all */
    for (fw_size_t i = 0; i < cap; ++i) {
        if (i == 0) {
            TEST_ASSERT(fw_pool_free(&pool, reallocated) == FW_OK, "Free must succeed");
        } else {
            TEST_ASSERT(fw_pool_free(&pool, blocks[i]) == FW_OK, "Free must succeed");
        }
    }
    TEST_ASSERT(fw_pool_available(&pool) == cap, "All blocks returned");

    /* Out of bounds pointer rejection */
    uint8_t external_var = 42;
    TEST_ASSERT(fw_pool_free(&pool, &external_var) == FW_ERR_INVALID_ARG, "Foreign ptr must be rejected");

    return FW_OK;
}

/* ========================================================================== */
/* Test 4: Linear Bump Arena with Scoped Rewind                               */
/* ========================================================================== */
static fw_status_t test_memory_arena(void) {
    FW_ALIGNED(16) static uint8_t arena_mem[512];
    fw_arena_t arena;

    TEST_ASSERT(fw_arena_init(&arena, arena_mem, sizeof(arena_mem)) == FW_OK, "Arena init ok");
    TEST_ASSERT(fw_arena_available(&arena) == 512, "Full capacity available");

    /* CRITICAL SAFETY TEST: Integer overflow in allocation size */
    TEST_ASSERT(fw_arena_alloc(&arena, (fw_size_t)-16, 8) == FW_NULL, "Massive overflow size rejected");

    /* Allocation 1: 64 bytes */
    void *p1 = fw_arena_alloc(&arena, 64, 8);
    TEST_ASSERT(p1 != FW_NULL, "p1 allocated");
    TEST_ASSERT(fw_arena_used(&arena) == 64, "Used should be 64");

    /* Save mark */
    fw_arena_mark_t mark = fw_arena_mark(&arena);

    /* Allocation 2: 128 bytes */
    void *p2 = fw_arena_alloc(&arena, 128, 8);
    TEST_ASSERT(p2 != FW_NULL, "p2 allocated");
    TEST_ASSERT(fw_arena_used(&arena) == 192, "Used should be 192");
    TEST_ASSERT(fw_arena_peak(&arena) == 192, "Peak should be 192");

    /* Rewind back to mark */
    fw_arena_rewind(&arena, mark);
    TEST_ASSERT(fw_arena_used(&arena) == 64, "Used reverted to 64");
    TEST_ASSERT(fw_arena_peak(&arena) == 192, "Peak watermark preserved");

    /* Reset completely */
    fw_arena_reset(&arena);
    TEST_ASSERT(fw_arena_used(&arena) == 0, "Used should be 0 after reset");

    return FW_OK;
}

/* ========================================================================== */
/* Test 5: Safe Bounded Buffer                                                */
/* ========================================================================== */
static fw_status_t test_memory_buffer(void) {
    uint8_t raw[16];
    fw_buffer_t buf;

    TEST_ASSERT(fw_buffer_init(&buf, raw, sizeof(raw)) == FW_OK, "Buffer init ok");
    TEST_ASSERT(fw_buffer_is_empty(&buf), "Buffer initially empty");
    TEST_ASSERT(fw_buffer_remaining(&buf) == 16, "Remaining is 16");

    /* CRITICAL SAFETY TEST: Integer overflow on count */
    TEST_ASSERT(fw_buffer_append(&buf, "X", (fw_size_t)-1) == FW_ERR_BUFFER_OVERFLOW, "Massive overflow count rejected");

    const char *msg = "ZeroEmb";
    TEST_ASSERT(fw_buffer_append(&buf, msg, 7) == FW_OK, "Append 7 bytes ok");
    TEST_ASSERT(buf.length == 7, "Length is 7");

    fw_span_t span = fw_buffer_as_span(&buf);
    TEST_ASSERT(span.length == 7, "Span length matches");
    TEST_ASSERT(memcmp(span.data, "ZeroEmb", 7) == 0, "Content matches");

    /* Try to append 10 bytes -> total 17 > 16 -> should overflow */
    uint8_t extra[10] = {0};
    TEST_ASSERT(fw_buffer_append(&buf, extra, 10) == FW_ERR_BUFFER_OVERFLOW, "Overflow rejected");
    TEST_ASSERT(buf.length == 7, "Buffer untouched on overflow failure");

    fw_buffer_clear(&buf);
    TEST_ASSERT(fw_buffer_is_empty(&buf), "Buffer cleared");

    return FW_OK;
}

/* ========================================================================== */
/* Test 6: Lock-Free SPSC RingBuffer (ISR-Safe)                               */
/* ========================================================================== */
static fw_status_t test_spsc_ringbuffer(void) {
    uint8_t storage[8];
    fw_spsc_t q;

    TEST_ASSERT(fw_spsc_init(&q, storage, 7) == FW_ERR_INVALID_ARG, "Non-power-of-2 capacity rejected");
    TEST_ASSERT(fw_spsc_init(&q, storage, 8) == FW_OK, "Init power-of-2 capacity ok");
    TEST_ASSERT(fw_spsc_is_empty(&q), "Initially empty");
    TEST_ASSERT(fw_spsc_available(&q) == 8, "8 slots available");

    /* Push 8 items to fill */
    for (uint8_t i = 1; i <= 8; ++i) {
        TEST_ASSERT(fw_spsc_push(&q, i) == FW_OK, "Push ok");
    }
    TEST_ASSERT(fw_spsc_is_full(&q), "Queue must be full");
    TEST_ASSERT(fw_spsc_push(&q, 99) == FW_ERR_BUFFER_OVERFLOW, "Overflow rejected");

    /* Pop 4 items */
    for (uint8_t i = 1; i <= 4; ++i) {
        uint8_t val = 0;
        TEST_ASSERT(fw_spsc_pop(&q, &val) == FW_OK, "Pop ok");
        TEST_ASSERT(val == i, "FIFO order preserved");
    }
    TEST_ASSERT(!fw_spsc_is_full(&q), "No longer full");
    TEST_ASSERT(fw_spsc_count(&q) == 4, "4 items remain");

    /* Push 4 more to wrap around boundary */
    for (uint8_t i = 9; i <= 12; ++i) {
        TEST_ASSERT(fw_spsc_push(&q, i) == FW_OK, "Push wrapped ok");
    }
    TEST_ASSERT(fw_spsc_is_full(&q), "Full again after wrap");

    /* Read remaining in bulk */
    uint8_t out[8] = {0};
    fw_size_t read_bytes = fw_spsc_read(&q, out, sizeof(out));
    TEST_ASSERT(read_bytes == 8, "Read 8 bytes in bulk");
    TEST_ASSERT(out[0] == 5 && out[1] == 6 && out[2] == 7 && out[3] == 8, "First batch correct");
    TEST_ASSERT(out[4] == 9 && out[5] == 10 && out[6] == 11 && out[7] == 12, "Wrapped batch correct");
    TEST_ASSERT(fw_spsc_is_empty(&q), "Queue empty after full read");

    return FW_OK;
}

/* ========================================================================== */
/* Test 7: ZeroWire Stream Resynchronization across Noise                     */
/* ========================================================================== */
static fw_status_t test_zerowire_stream_resync(void) {
    /* Create frame */
    fw_zerowire_frame_t tx_frame;
    tx_frame.seq = 42;
    tx_frame.msg_id = 0x5A;
    tx_frame.length = 5;
    memcpy(tx_frame.payload, "HELLO", 5);

    uint8_t frame_buf[32];
    fw_span_t frame_span = FW_SPAN_FROM_ARRAY(frame_buf);
    fw_size_t frame_len = fw_zerowire_encode(&tx_frame, frame_span);
    TEST_ASSERT(frame_len > 0, "Encode ok");

    /* Create stream with 7 noise bytes prepended */
    uint8_t stream_buf[64];
    memset(stream_buf, 0xFF, 7); /* Line noise / garbage */
    memcpy(stream_buf + 7, frame_buf, frame_len);

    fw_cspan_t stream_cspan = fw_cspan_make(stream_buf, 7 + frame_len);
    fw_zerowire_frame_t rx_frame;
    fw_size_t consumed = 0;

    fw_status_t status = fw_zerowire_stream_sync(stream_cspan, &rx_frame, &consumed);
    TEST_ASSERT(status == FW_OK, "Stream resync found valid frame");
    TEST_ASSERT(consumed == 7 + frame_len, "Consumed exactly noise + frame size");
    TEST_ASSERT(rx_frame.seq == 42, "Seq matches");
    TEST_ASSERT(rx_frame.msg_id == 0x5A, "MsgID matches");
    TEST_ASSERT(memcmp(rx_frame.payload, "HELLO", 5) == 0, "Payload matches");

    return FW_OK;
}

int main(void) {
    printf("\n--- Running ZeroEmbedded Hardened Test Suite ---\n");

    if (test_primitives() != FW_OK) return 1;
    if (test_span_and_string_view() != FW_OK) return 1;
    if (test_memory_pool() != FW_OK) return 1;
    if (test_memory_arena() != FW_OK) return 1;
    if (test_memory_buffer() != FW_OK) return 1;
    if (test_spsc_ringbuffer() != FW_OK) return 1;
    if (test_zerowire_stream_resync() != FW_OK) return 1;

    printf("\n=== All %d tests passed successfully! ===\n\n", g_tests_passed);
    return 0;
}
