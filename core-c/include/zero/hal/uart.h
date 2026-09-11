#ifndef ZERO_HAL_UART_H
#define ZERO_HAL_UART_H

/**
 * @file uart.h
 * @brief Asynchronous interrupt & DMA-driven UART abstraction for ZeroEmbedded.
 */

#include "../types.h"
#include "../result.h"
#include "../span.h"
#include "../sync/spsc.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FW_UART_PARITY_NONE = 0,
    FW_UART_PARITY_EVEN = 1,
    FW_UART_PARITY_ODD  = 2
} fw_uart_parity_t;

typedef enum {
    FW_UART_STOP_1 = 1,
    FW_UART_STOP_2 = 2
} fw_uart_stop_t;

typedef struct {
    uint32_t         baud_rate;
    fw_uart_parity_t parity;
    fw_uart_stop_t   stop_bits;
    fw_bool_t        enable_dma;
} fw_uart_config_t;

typedef struct {
    void            *hardware_instance; /* Pointer to silicon UART peripheral */
    fw_spsc_t       *tx_queue;           /* Optional SPSC TX ringbuffer */
    fw_spsc_t       *rx_queue;           /* Optional SPSC RX ringbuffer */
    fw_bool_t        is_initialized;
} fw_uart_t;

fw_status_t fw_uart_init(fw_uart_t *uart, void *hw_inst, const fw_uart_config_t *cfg, fw_spsc_t *tx_q, fw_spsc_t *rx_q);
fw_size_t   fw_uart_write(fw_uart_t *uart, fw_cspan_t data);
fw_size_t   fw_uart_read(fw_uart_t *uart, fw_span_t buffer);

/** Called inside UART IRQ handler to service interrupts */
FW_ISR void fw_uart_irq_handler(fw_uart_t *uart);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_UART_H */
