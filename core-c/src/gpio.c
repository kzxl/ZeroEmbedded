#include "zero/hal/gpio.h"

/* Virtual GPIO pin state storage for Host Simulation & Unit Testing */
static uint16_t s_virtual_gpio_state[8] = {0};
static uint16_t s_virtual_gpio_dir[8]   = {0}; /* 1 = Output, 0 = Input */

fw_status_t fw_gpio_init(fw_gpio_t pin) {
    if (pin.port >= 8 || pin.pin >= 16) {
        return FW_ERR_INVALID_ARG;
    }

    uint16_t mask = (uint16_t)(1U << pin.pin);
    if (pin.mode == FW_GPIO_MODE_OUTPUT_PP || pin.mode == FW_GPIO_MODE_OUTPUT_OD) {
        s_virtual_gpio_dir[pin.port] |= mask;
    } else {
        s_virtual_gpio_dir[pin.port] &= (uint16_t)~mask;
    }

    return FW_OK;
}

void fw_gpio_write(fw_gpio_t pin, fw_bool_t state) {
    if (pin.port >= 8 || pin.pin >= 16) {
        return;
    }

    uint16_t mask = (uint16_t)(1U << pin.pin);
    if (state) {
        s_virtual_gpio_state[pin.port] |= mask;
    } else {
        s_virtual_gpio_state[pin.port] &= (uint16_t)~mask;
    }
}

fw_bool_t fw_gpio_read(fw_gpio_t pin) {
    if (pin.port >= 8 || pin.pin >= 16) {
        return FW_FALSE;
    }

    uint16_t mask = (uint16_t)(1U << pin.pin);
    return (s_virtual_gpio_state[pin.port] & mask) ? FW_TRUE : FW_FALSE;
}

void fw_gpio_toggle(fw_gpio_t pin) {
    if (pin.port >= 8 || pin.pin >= 16) {
        return;
    }

    uint16_t mask = (uint16_t)(1U << pin.pin);
    s_virtual_gpio_state[pin.port] ^= mask;
}

/* ========================================================================== */
/* Host Mock Inspection Helpers (Useful for CI & Automated Testing)           */
/* ========================================================================== */

uint16_t fw_gpio_mock_get_port(fw_gpio_port_t port) {
    if (port >= 8) return 0;
    return s_virtual_gpio_state[port];
}

void fw_gpio_mock_set_input(fw_gpio_port_t port, fw_gpio_pin_t pin, fw_bool_t state) {
    if (port >= 8 || pin >= 16) return;
    uint16_t mask = (uint16_t)(1U << pin);
    if (state) {
        s_virtual_gpio_state[port] |= mask;
    } else {
        s_virtual_gpio_state[port] &= (uint16_t)~mask;
    }
}
