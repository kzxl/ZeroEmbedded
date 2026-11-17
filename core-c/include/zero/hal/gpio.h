#ifndef ZERO_HAL_GPIO_H
#define ZERO_HAL_GPIO_H

/**
 * @file gpio.h
 * @brief Type-safe zero-cost GPIO abstraction for ZeroEmbedded.
 */

#include "zero/types.h"
#include "zero/result.h"
#include "zero/attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FW_GPIO_PORT_A = 0,
    FW_GPIO_PORT_B = 1,
    FW_GPIO_PORT_C = 2,
    FW_GPIO_PORT_D = 3,
    FW_GPIO_PORT_E = 4,
    FW_GPIO_PORT_F = 5,
    FW_GPIO_PORT_G = 6,
    FW_GPIO_PORT_H = 7
} fw_gpio_port_t;

typedef enum {
    FW_GPIO_PIN_0  = 0,
    FW_GPIO_PIN_1  = 1,
    FW_GPIO_PIN_2  = 2,
    FW_GPIO_PIN_3  = 3,
    FW_GPIO_PIN_4  = 4,
    FW_GPIO_PIN_5  = 5,
    FW_GPIO_PIN_6  = 6,
    FW_GPIO_PIN_7  = 7,
    FW_GPIO_PIN_8  = 8,
    FW_GPIO_PIN_9  = 9,
    FW_GPIO_PIN_10 = 10,
    FW_GPIO_PIN_11 = 11,
    FW_GPIO_PIN_12 = 12,
    FW_GPIO_PIN_13 = 13,
    FW_GPIO_PIN_14 = 14,
    FW_GPIO_PIN_15 = 15
} fw_gpio_pin_t;

typedef enum {
    FW_GPIO_MODE_INPUT     = 0,
    FW_GPIO_MODE_OUTPUT_PP = 1, /* Push-Pull */
    FW_GPIO_MODE_OUTPUT_OD = 2, /* Open-Drain */
    FW_GPIO_MODE_AF_PP     = 3, /* Alternate Function Push-Pull */
    FW_GPIO_MODE_AF_OD     = 4, /* Alternate Function Open-Drain */
    FW_GPIO_MODE_ANALOG    = 5
} fw_gpio_mode_t;

typedef enum {
    FW_GPIO_PULL_NONE = 0,
    FW_GPIO_PULL_UP   = 1,
    FW_GPIO_PULL_DOWN = 2
} fw_gpio_pull_t;

typedef enum {
    FW_GPIO_SPEED_LOW       = 0,
    FW_GPIO_SPEED_MEDIUM    = 1,
    FW_GPIO_SPEED_HIGH      = 2,
    FW_GPIO_SPEED_VERY_HIGH = 3
} fw_gpio_speed_t;

/** Packed 16-bit type-safe pin descriptor (Zero-allocation, ISO C11 bitfields) */
typedef struct {
    unsigned int port : 4;
    unsigned int pin  : 4;
    unsigned int mode : 4;
    unsigned int pull : 2;
    unsigned int speed: 2;
} fw_gpio_t;

#define FW_GPIO_PIN(port_id, pin_id) \
    { (unsigned int)(port_id), (unsigned int)(pin_id), (unsigned int)FW_GPIO_MODE_OUTPUT_PP, (unsigned int)FW_GPIO_PULL_NONE, (unsigned int)FW_GPIO_SPEED_HIGH }

/** Common hardware-level function prototypes implemented by silicon BSP */
fw_status_t fw_gpio_init(fw_gpio_t pin);
void fw_gpio_write(fw_gpio_t pin, fw_bool_t state);
fw_bool_t fw_gpio_read(fw_gpio_t pin);
void fw_gpio_toggle(fw_gpio_t pin);

/** Host Simulation & CI Mock Inspection Helpers */
uint16_t fw_gpio_mock_get_port(fw_gpio_port_t port);
void fw_gpio_mock_set_input(fw_gpio_port_t port, fw_gpio_pin_t pin, fw_bool_t state);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_GPIO_H */
