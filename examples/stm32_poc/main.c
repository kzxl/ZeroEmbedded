/**
 * @file main.c
 * @brief ZeroEmbedded Phase 10: Real Firmware PoC on ARM Cortex-M / STM32.
 * Demonstrates: Init -> Memory Pool -> ISR RingBuffer -> ZeroWire Processing -> HAL.
 */

#include <stdio.h>
#include <string.h>
#include "zero/zero.h"
#include "zero/hal/gpio.h"
#include "zero/protocol/zerowire.h"

/* 1. Static resources layout (Zero dynamic heap) */
#define TELEMETRY_POOL_BLOCKS 8
#define TELEMETRY_BLOCK_SIZE  32
FW_ALIGNED(8) static uint8_t s_telemetry_mem[TELEMETRY_POOL_BLOCKS * TELEMETRY_BLOCK_SIZE];
static fw_pool_t s_telemetry_pool;

#define UART_RX_RING_SIZE 64
static uint8_t s_uart_rx_raw[UART_RX_RING_SIZE];
static fw_spsc_t s_uart_rx_queue;

static const fw_gpio_t LED_STATUS = FW_GPIO_PIN(FW_GPIO_PORT_A, FW_GPIO_PIN_5);

/* 2. Simulated Hardware Interrupt Routine (Phase 5: FW_ISR) */
FW_ISR void USART2_IRQHandler(void) {
    /* Simulated incoming byte from ZeroPlatform host */
    uint8_t rx_byte = 0xAA;
    fw_spsc_push(&s_uart_rx_queue, rx_byte);
}

/* 3. Main Firmware Entry Point (FW_INIT -> FW_NORMAL loop) */
FW_INIT fw_status_t firmware_init(void) {
    /* Init memory pool */
    FW_CHECK(fw_pool_init(
        &s_telemetry_pool,
        s_telemetry_mem,
        sizeof(s_telemetry_mem),
        TELEMETRY_BLOCK_SIZE,
        8
    ));

    /* Init ISR ring buffer */
    FW_CHECK(fw_spsc_init(&s_uart_rx_queue, s_uart_rx_raw, UART_RX_RING_SIZE));

    return FW_OK;
}

FW_NORMAL int main(void) {
    printf("[FIRMWARE] Booting ZeroEmbedded PoC...\n");

    if (firmware_init() != FW_OK) {
        printf("[ERROR] Firmware init failed!\n");
        return 1;
    }
    printf("[FIRMWARE] Init OK. Telemetry pool ready (%zu blocks).\n", fw_pool_capacity(&s_telemetry_pool));

    /* Simulate host sending a ZeroWire packet */
    fw_zerowire_frame_t host_request;
    host_request.seq = 1;
    host_request.msg_id = 0x10; /* GET_STATUS */
    host_request.length = 4;
    memcpy(host_request.payload, "PING", 4);

    uint8_t wire_buffer[64];
    fw_span_t wire_span = FW_SPAN_FROM_ARRAY(wire_buffer);
    fw_size_t encoded_bytes = fw_zerowire_encode(&host_request, wire_span);
    printf("[FIRMWARE] Encoded ZeroWire frame from host (%zu bytes)\n", encoded_bytes);

    /* Firmware decodes the frame */
    fw_cspan_t rx_cspan = fw_cspan_make(wire_buffer, encoded_bytes);
    fw_zerowire_frame_t decoded_frame;
    fw_status_t decode_status = fw_zerowire_decode(rx_cspan, &decoded_frame);

    if (decode_status == FW_OK) {
        printf("[FIRMWARE] Decoded frame: MsgID=0x%02X, Seq=%d, Len=%d, Payload='%.*s'\n",
            decoded_frame.msg_id,
            decoded_frame.seq,
            decoded_frame.length,
            decoded_frame.length,
            decoded_frame.payload
        );

        /* Allocate telemetry block from pool */
        void *telemetry = fw_pool_alloc(&s_telemetry_pool);
        if (telemetry != FW_NULL) {
            snprintf((char*)telemetry, TELEMETRY_BLOCK_SIZE, "STATUS_OK_UPTIME_%d", 100);
            printf("[FIRMWARE] Generated telemetry: %s\n", (char*)telemetry);
            fw_pool_free(&s_telemetry_pool, telemetry);
        }
    }

    printf("[FIRMWARE] End-to-end PoC completed successfully.\n");
    return 0;
}
