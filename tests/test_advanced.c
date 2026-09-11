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
/* 1. Test Power Management & Duty-Cycle Metrics                              */
/* ========================================================================== */
static fw_status_t test_power_management(void) {
    fw_pm_init();

    fw_delay_millis(5); /* 5ms active */
    fw_pm_enter_sleep(10); /* 10ms sleep */

    fw_pm_metrics_t metrics;
    fw_pm_get_metrics(&metrics);

    TEST_ASSERT(metrics.sleep_count == 1, "Sleep count should be 1");
    TEST_ASSERT(metrics.total_sleep_us >= 5000, "Sleep time >= 5ms");
    TEST_ASSERT(metrics.total_active_us >= 2000, "Active time >= 2ms");
    TEST_ASSERT(metrics.cpu_load_permille > 0 && metrics.cpu_load_permille < 1000,
                "CPU load should be between 0 and 100%");

    return FW_OK;
}

/* ========================================================================== */
/* 2. Test Hardware Cycle Counter (DWT / RDTSC)                               */
/* ========================================================================== */
static fw_status_t test_dwt_cycles(void) {
    fw_dwt_init();

    uint32_t c0 = fw_dwt_get_cycles();
    volatile uint32_t dummy = 0;
    for (int i = 0; i < 1000; ++i) {
        dummy += (uint32_t)i;
    }
    uint32_t c1 = fw_dwt_get_cycles();

    uint32_t elapsed = fw_dwt_elapsed_cycles(c0, c1);
    TEST_ASSERT(elapsed > 0, "DWT elapsed cycles must advance");

    /* Test overflow wrap-around math */
    uint32_t wrap_start = 0xFFFFFFF0U;
    uint32_t wrap_end   = 0x00000020U;
    uint32_t wrap_elapsed = fw_dwt_elapsed_cycles(wrap_start, wrap_end);
    TEST_ASSERT(wrap_elapsed == 48, "Cycle counter wrap math must equal 48");

    return FW_OK;
}

/* ========================================================================== */
/* 3. Test Command Table & RPC Dispatcher                                     */
/* ========================================================================== */

#define CMD_PING      0x10
#define CMD_GET_INFO  0x20

static fw_status_t handle_ping(uint8_t seq, fw_cspan_t req, fw_span_t resp, fw_size_t *out_len) {
    (void)seq;
    (void)req;
    const char *pong = "PONG";
    memcpy(resp.data, pong, 4);
    *out_len = 4;
    return FW_OK;
}

static fw_status_t handle_get_info(uint8_t seq, fw_cspan_t req, fw_span_t resp, fw_size_t *out_len) {
    (void)seq;
    (void)req;
    const char *info = "ZERO_V0.3";
    memcpy(resp.data, info, 9);
    *out_len = 9;
    return FW_OK;
}

static fw_status_t test_rpc_dispatcher(void) {
    static const fw_cmd_entry_t s_cmds[] = {
        { CMD_PING,     0, handle_ping },
        { CMD_GET_INFO, 2, handle_get_info } /* Requires at least 2 bytes payload */
    };

    fw_cmd_table_t table;
    TEST_ASSERT(fw_cmd_table_init(&table, s_cmds, 2) == FW_OK, "Init command table ok");

    /* 3.1 Test Valid PING request */
    fw_zerowire_frame_t req;
    req.seq = 7;
    req.msg_id = CMD_PING;
    req.length = 0;

    fw_zerowire_frame_t resp;
    fw_bool_t has_resp = FW_FALSE;
    TEST_ASSERT(fw_cmd_dispatch(&table, &req, &resp, &has_resp) == FW_OK, "Dispatch PING ok");
    TEST_ASSERT(has_resp == FW_TRUE, "PING generates response");
    TEST_ASSERT(resp.seq == 7, "Response seq matches req seq (7)");
    TEST_ASSERT(resp.msg_id == (CMD_PING | 0x80), "Response msg_id has response bit 0x90");
    TEST_ASSERT(resp.length == 4, "Payload length is 4");
    TEST_ASSERT(memcmp(resp.payload, "PONG", 4) == 0, "Payload is PONG");

    /* 3.2 Test Unhandled MsgID rejection */
    fw_zerowire_frame_t unhandled_req = { 1, 0x55, 0, {0} };
    TEST_ASSERT(fw_cmd_dispatch(&table, &unhandled_req, &resp, &has_resp) == FW_ERR_NOT_FOUND,
                "Unknown MsgID rejected");

    /* 3.3 Test Minimum Length enforcement */
    fw_zerowire_frame_t short_req = { 2, CMD_GET_INFO, 1, {0} }; /* min is 2 */
    TEST_ASSERT(fw_cmd_dispatch(&table, &short_req, &resp, &has_resp) == FW_ERR_INVALID_ARG,
                "Short payload rejected");

    /* 3.4 Test Full Stream Processing: Noise + Valid PING Frame -> Encoded Response */
    uint8_t ping_frame_raw[32];
    fw_span_t ping_span = FW_SPAN_FROM_ARRAY(ping_frame_raw);
    fw_size_t ping_len = fw_zerowire_encode(&req, ping_span);
    TEST_ASSERT(ping_len > 0, "Encode ping ok");

    /* Create stream buffer with 4 noise bytes prepended */
    uint8_t stream_buf[64];
    memset(stream_buf, 0xEE, 4);
    memcpy(stream_buf + 4, ping_frame_raw, ping_len);

    uint8_t out_reply[64];
    fw_span_t out_reply_span = FW_SPAN_FROM_ARRAY(out_reply);
    fw_size_t consumed = 0;
    fw_size_t out_reply_len = 0;

    fw_cspan_t stream_cspan = fw_cspan_make(stream_buf, 4 + ping_len);
    fw_status_t proc_st = fw_cmd_process_stream(&table, stream_cspan, &consumed, out_reply_span, &out_reply_len);

    TEST_ASSERT(proc_st == FW_OK, "Stream RPC processing succeeded");
    TEST_ASSERT(consumed == 4 + ping_len, "Consumed exactly noise + request frame");
    TEST_ASSERT(out_reply_len > 0, "Generated reply binary frame");

    /* Verify the generated reply frame */
    fw_zerowire_frame_t decoded_reply;
    TEST_ASSERT(fw_zerowire_decode(fw_cspan_make(out_reply, out_reply_len), &decoded_reply) == FW_OK,
                "Reply frame CRC16 verified");
    TEST_ASSERT(decoded_reply.msg_id == 0x90, "Reply MsgID is 0x90");
    TEST_ASSERT(memcmp(decoded_reply.payload, "PONG", 4) == 0, "Reply content is PONG");

    return FW_OK;
}

int main(void) {
    printf("\n--- Running ZeroEmbedded Advanced Features Test Suite ---\n");

    if (test_power_management() != FW_OK) return 1;
    if (test_dwt_cycles() != FW_OK) return 1;
    if (test_rpc_dispatcher() != FW_OK) return 1;

    printf("\n=== All %d Advanced tests passed successfully! ===\n\n", g_tests_passed);
    return 0;
}
