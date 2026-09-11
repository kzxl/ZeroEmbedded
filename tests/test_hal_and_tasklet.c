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
/* Test 1: GPIO Virtual Hardware Pin & Manipulation                           */
/* ========================================================================== */
static fw_status_t test_gpio_driver(void) {
    fw_gpio_t led = FW_GPIO_PIN(FW_GPIO_PORT_A, FW_GPIO_PIN_5);
    TEST_ASSERT(fw_gpio_init(led) == FW_OK, "GPIO Init PA5 output ok");

    fw_gpio_write(led, FW_TRUE);
    TEST_ASSERT(fw_gpio_read(led) == FW_TRUE, "PA5 should be HIGH");

    fw_gpio_write(led, FW_FALSE);
    TEST_ASSERT(fw_gpio_read(led) == FW_FALSE, "PA5 should be LOW");

    fw_gpio_toggle(led);
    TEST_ASSERT(fw_gpio_read(led) == FW_TRUE, "PA5 should be HIGH after toggle");

    fw_gpio_toggle(led);
    TEST_ASSERT(fw_gpio_read(led) == FW_FALSE, "PA5 should be LOW after second toggle");

    /* Out of bounds port/pin rejection */
    fw_gpio_t invalid_pin = { 15, 15, 0, 0, 0 };
    TEST_ASSERT(fw_gpio_init(invalid_pin) == FW_ERR_INVALID_ARG, "Invalid port rejected");

    return FW_OK;
}

/* ========================================================================== */
/* Test 2: Timer & Non-Blocking Timeouts                                      */
/* ========================================================================== */
static fw_status_t test_timer_and_timeouts(void) {
    uint32_t t0 = fw_timer_get_millis();
    uint64_t u0 = fw_timer_get_micros();

    fw_delay_millis(15);

    uint32_t t1 = fw_timer_get_millis();
    uint64_t u1 = fw_timer_get_micros();

    TEST_ASSERT(t1 >= t0, "Monotonic millis must advance");
    TEST_ASSERT(u1 > u0, "Monotonic micros must advance");

    /* Test Non-blocking fw_timeout_t */
    fw_timeout_t timeout;
    fw_timeout_start(&timeout, 20);
    TEST_ASSERT(!fw_timeout_is_expired(&timeout), "Timeout should not expire immediately");
    TEST_ASSERT(fw_timeout_remaining_ms(&timeout) <= 20, "Remaining <= 20ms");

    fw_delay_millis(25);
    TEST_ASSERT(fw_timeout_is_expired(&timeout), "Timeout should be expired after 25ms");
    TEST_ASSERT(fw_timeout_remaining_ms(&timeout) == 0, "Remaining is 0 after expiration");

    return FW_OK;
}

/* ========================================================================== */
/* Test 3: UART Asynchronous Driver & SPSC Buffering                          */
/* ========================================================================== */
/* External mock helpers from uart.c */
extern void fw_uart_mock_inject_rx(fw_uart_t *uart, uint8_t byte);
extern fw_bool_t fw_uart_mock_extract_tx(fw_uart_t *uart, uint8_t *out_byte);

static fw_status_t test_uart_driver(void) {
    uint8_t tx_mem[32];
    uint8_t rx_mem[32];
    fw_spsc_t tx_q;
    fw_spsc_t rx_q;

    TEST_ASSERT(fw_spsc_init(&tx_q, tx_mem, 32) == FW_OK, "Init TX SPSC ok");
    TEST_ASSERT(fw_spsc_init(&rx_q, rx_mem, 32) == FW_OK, "Init RX SPSC ok");

    fw_uart_t uart;
    fw_uart_config_t cfg = { 115200, FW_UART_PARITY_NONE, FW_UART_STOP_1, FW_FALSE };

    TEST_ASSERT(fw_uart_init(&uart, (void*)0x40004400, &cfg, &tx_q, &rx_q) == FW_OK, "UART Init ok");

    /* Test TX Write */
    const char *msg = "HELLO_UART";
    fw_cspan_t tx_span = fw_cspan_make(msg, 10);
    fw_size_t written = fw_uart_write(&uart, tx_span);
    TEST_ASSERT(written == 10, "Written 10 bytes to UART TX queue");

    /* Extract from TX queue (simulate hardware transmission) */
    uint8_t sent_byte = 0;
    TEST_ASSERT(fw_uart_mock_extract_tx(&uart, &sent_byte) == FW_TRUE, "Extract 1st byte");
    TEST_ASSERT(sent_byte == 'H', "Extracted 'H'");

    /* Test RX Receive */
    fw_uart_mock_inject_rx(&uart, 0xAA);
    fw_uart_mock_inject_rx(&uart, 0x55);

    uint8_t rx_buf[8] = {0};
    fw_span_t rx_span = FW_SPAN_FROM_ARRAY(rx_buf);
    fw_size_t read_bytes = fw_uart_read(&uart, rx_span);
    TEST_ASSERT(read_bytes == 2, "Read 2 bytes from UART RX");
    TEST_ASSERT(rx_buf[0] == 0xAA && rx_buf[1] == 0x55, "RX bytes match injected values");

    return FW_OK;
}

/* ========================================================================== */
/* Test 4: Cooperative Tasklet Event Dispatcher                               */
/* ========================================================================== */
static int s_tasklet_exec_count = 0;
static uint32_t s_last_tasklet_arg = 0;

static void sample_tasklet_handler(void *context, uint32_t arg) {
    (void)context;
    s_tasklet_exec_count++;
    s_last_tasklet_arg = arg;
}

static fw_status_t test_tasklet_dispatcher(void) {
    fw_tasklet_item_t storage[8];
    fw_tasklet_queue_t queue;

    TEST_ASSERT(fw_tasklet_queue_init(&queue, storage, 8) == FW_OK, "Tasklet init ok");
    TEST_ASSERT(fw_tasklet_is_empty(&queue), "Queue initially empty");

    /* Post 3 tasklets */
    s_tasklet_exec_count = 0;
    TEST_ASSERT(fw_tasklet_post(&queue, sample_tasklet_handler, FW_NULL, 101) == FW_OK, "Post 1 ok");
    TEST_ASSERT(fw_tasklet_post(&queue, sample_tasklet_handler, FW_NULL, 102) == FW_OK, "Post 2 ok");
    TEST_ASSERT(fw_tasklet_post(&queue, sample_tasklet_handler, FW_NULL, 103) == FW_OK, "Post 3 ok");

    TEST_ASSERT(fw_tasklet_count(&queue) == 3, "Count is 3");

    /* Dispatch one */
    TEST_ASSERT(fw_tasklet_dispatch_one(&queue) == FW_TRUE, "Dispatched 1");
    TEST_ASSERT(s_tasklet_exec_count == 1, "Handler executed once");
    TEST_ASSERT(s_last_tasklet_arg == 101, "First arg executed FIFO (101)");

    /* Dispatch all remaining */
    fw_size_t dispatched = fw_tasklet_dispatch_all(&queue);
    TEST_ASSERT(dispatched == 2, "Dispatched remaining 2");
    TEST_ASSERT(s_tasklet_exec_count == 3, "Total executions is 3");
    TEST_ASSERT(s_last_tasklet_arg == 103, "Last arg executed was 103");
    TEST_ASSERT(fw_tasklet_is_empty(&queue), "Queue is empty now");

    return FW_OK;
}

/* ========================================================================== */
/* Test 5: RTOS Bare-metal Fallback Primitives                                */
/* ========================================================================== */
static fw_status_t test_rtos_primitives(void) {
    fw_mutex_handle_t mutex = FW_NULL;
    TEST_ASSERT(fw_mutex_init(&mutex) == FW_OK, "Mutex init ok");
    TEST_ASSERT(fw_mutex_lock(mutex, 100) == FW_OK, "Mutex lock ok");
    TEST_ASSERT(fw_mutex_unlock(mutex) == FW_OK, "Mutex unlock ok");

    fw_sem_handle_t sem = FW_NULL;
    TEST_ASSERT(fw_sem_init(&sem, 1, 1) == FW_OK, "Semaphore init ok");
    TEST_ASSERT(fw_sem_take(sem, 50) == FW_OK, "Semaphore take ok");
    TEST_ASSERT(fw_sem_give(sem) == FW_OK, "Semaphore give ok");

    return FW_OK;
}

int main(void) {
    printf("\n--- Running ZeroEmbedded HAL & Tasklet Test Suite ---\n");

    if (test_gpio_driver() != FW_OK) return 1;
    if (test_timer_and_timeouts() != FW_OK) return 1;
    if (test_uart_driver() != FW_OK) return 1;
    if (test_tasklet_dispatcher() != FW_OK) return 1;
    if (test_rtos_primitives() != FW_OK) return 1;

    printf("\n=== All %d HAL & Tasklet tests passed successfully! ===\n\n", g_tests_passed);
    return 0;
}
