#include "zero/hal/uart.h"

fw_status_t fw_uart_init(
    fw_uart_t *uart,
    void *hw_inst,
    const fw_uart_config_t *cfg,
    fw_spsc_t *tx_q,
    fw_spsc_t *rx_q
) {
    if (uart == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    (void)cfg; /* Config parameters applied during hardware peripheral setup */

    uart->hardware_instance = hw_inst;
    uart->tx_queue = tx_q;
    uart->rx_queue = rx_q;
    uart->is_initialized = FW_TRUE;

    return FW_OK;
}

fw_size_t fw_uart_write(fw_uart_t *uart, fw_cspan_t data) {
    if (uart == FW_NULL || !uart->is_initialized || data.data == FW_NULL || data.length == 0) {
        return 0;
    }

    if (uart->tx_queue != FW_NULL) {
        return fw_spsc_write(uart->tx_queue, data.data, data.length);
    }

    return 0;
}

fw_size_t fw_uart_read(fw_uart_t *uart, fw_span_t buffer) {
    if (uart == FW_NULL || !uart->is_initialized || buffer.data == FW_NULL || buffer.length == 0) {
        return 0;
    }

    if (uart->rx_queue != FW_NULL) {
        return fw_spsc_read(uart->rx_queue, buffer.data, buffer.length);
    }

    return 0;
}

FW_ISR void fw_uart_irq_handler(fw_uart_t *uart) {
    if (uart == FW_NULL || !uart->is_initialized) {
        return;
    }

    /* Common hardware UART ISR service routine:
     * 1. If RXNE (RX register not empty), read byte and push to rx_queue
     * 2. If TXE (TX register empty) and tx_queue has data, pop byte and write to data register
     *
     * In real embedded silicon, read USART_SR & USART_DR registers.
     */
}

/* ========================================================================== */
/* Host Simulation & Loopback Helpers                                         */
/* ========================================================================== */

void fw_uart_mock_inject_rx(fw_uart_t *uart, uint8_t byte) {
    if (uart != FW_NULL && uart->rx_queue != FW_NULL) {
        fw_spsc_push(uart->rx_queue, byte);
    }
}

fw_bool_t fw_uart_mock_extract_tx(fw_uart_t *uart, uint8_t *out_byte) {
    if (uart != FW_NULL && uart->tx_queue != FW_NULL && out_byte != FW_NULL) {
        return fw_spsc_pop(uart->tx_queue, out_byte) == FW_OK ? FW_TRUE : FW_FALSE;
    }
    return FW_FALSE;
}
