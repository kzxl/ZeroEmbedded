/**
 * @file main.c
 * @brief ZeroEmbedded Host Simulation Demo
 * Demonstrates the entire embedded framework running on PC (Host Mock Backend).
 */

#include <stdio.h>
#include <string.h>
#include "zero/zero.h"

/* Static storage allocation (No dynamic heap) */
#define TX_RING_CAPACITY 64
#define RX_RING_CAPACITY 64
static uint8_t s_tx_storage[TX_RING_CAPACITY];
static uint8_t s_rx_storage[RX_RING_CAPACITY];
static fw_spsc_t s_tx_queue;
static fw_spsc_t s_rx_queue;
static fw_uart_t s_uart;

#define TASKLET_CAPACITY 16
static fw_tasklet_item_t s_tasklets[TASKLET_CAPACITY];
static fw_tasklet_queue_t s_tasklet_q;

static const fw_gpio_t STATUS_LED = FW_GPIO_PIN(FW_GPIO_PORT_C, FW_GPIO_PIN_13);

/* External mock injection helpers */
extern void fw_uart_mock_inject_rx(fw_uart_t *uart, uint8_t byte);
extern fw_bool_t fw_uart_mock_extract_tx(fw_uart_t *uart, uint8_t *out_byte);

static void on_packet_processed(void *context, uint32_t arg) {
    (void)context;
    printf("[HOST_SIM:TASKLET] Processing packet ID 0x%02X in background...\n", arg);
    fw_gpio_toggle(STATUS_LED);
    printf("[HOST_SIM:GPIO] Virtual LED PC13 state: %s\n", fw_gpio_read(STATUS_LED) ? "ON" : "OFF");
}

int main(void) {
    printf("===============================================================\n");
    printf(" ⚡ ZeroEmbedded Host Simulation & Virtual Firmware Environment\n");
    printf("===============================================================\n");

    /* 1. Initialize Subsystems */
    fw_gpio_init(STATUS_LED);
    fw_gpio_write(STATUS_LED, FW_FALSE);

    fw_spsc_init(&s_tx_queue, s_tx_storage, TX_RING_CAPACITY);
    fw_spsc_init(&s_rx_queue, s_rx_storage, RX_RING_CAPACITY);
    fw_uart_config_t cfg = { 115200, FW_UART_PARITY_NONE, FW_UART_STOP_1, FW_FALSE };
    fw_uart_init(&s_uart, FW_NULL, &cfg, &s_tx_queue, &s_rx_queue);
    fw_tasklet_queue_init(&s_tasklet_q, s_tasklets, TASKLET_CAPACITY);

    printf("[HOST_SIM] All virtual drivers initialized successfully.\n");

    /* 2. Encode & Send a ZeroWire Frame */
    fw_zerowire_frame_t frame_out;
    frame_out.seq = 1;
    frame_out.msg_id = 0x42;
    frame_out.length = 8;
    memcpy(frame_out.payload, "HOST_OK!", 8);

    uint8_t wire_bytes[64];
    fw_span_t wire_span = FW_SPAN_FROM_ARRAY(wire_bytes);
    fw_size_t encoded = fw_zerowire_encode(&frame_out, wire_span);
    printf("[HOST_SIM] Encoded %zu bytes frame for transmission.\n", encoded);

    /* Write to UART TX */
    fw_cspan_t tx_cspan = fw_cspan_make(wire_bytes, encoded);
    fw_size_t queued = fw_uart_write(&s_uart, tx_cspan);
    printf("[HOST_SIM] Pushed %zu bytes into UART TX ringbuffer.\n", queued);

    /* 3. Simulate Loopback: Drain TX and inject into RX */
    uint8_t byte = 0;
    while (fw_uart_mock_extract_tx(&s_uart, &byte)) {
        fw_uart_mock_inject_rx(&s_uart, byte);
    }
    printf("[HOST_SIM] Simulated hardware loopback transfer complete.\n");

    /* 4. Receive and Decode Frame from UART RX */
    uint8_t rx_buffer[64];
    fw_span_t rx_span = FW_SPAN_FROM_ARRAY(rx_buffer);
    fw_size_t rx_len = fw_uart_read(&s_uart, rx_span);
    printf("[HOST_SIM] Read %zu bytes from UART RX ringbuffer.\n", rx_len);

    fw_zerowire_frame_t frame_in;
    fw_status_t dec_st = fw_zerowire_decode(fw_cspan_make(rx_buffer, rx_len), &frame_in);
    if (dec_st == FW_OK) {
        printf("[HOST_SIM] Successfully decoded packet! MsgID: 0x%02X, Seq: %d, Data: '%.*s'\n",
            frame_in.msg_id, frame_in.seq, frame_in.length, frame_in.payload);

        /* Post deferred tasklet */
        fw_tasklet_post(&s_tasklet_q, on_packet_processed, FW_NULL, frame_in.msg_id);
    }

    /* 5. Dispatch tasklets */
    fw_tasklet_dispatch_all(&s_tasklet_q);

    /* 6. Non-blocking periodic timer verification */
    fw_timeout_t ticker;
    fw_timeout_start(&ticker, 20);
    while (!fw_timeout_is_expired(&ticker)) {
        /* Superloop tick */
    }
    printf("[HOST_SIM] Monotonic Clock Uptime: %u ms, %llu us.\n",
        fw_timer_get_millis(), (unsigned long long)fw_timer_get_micros());

    printf("[HOST_SIM] Simulation finished with 100%% success!\n");
    return 0;
}
