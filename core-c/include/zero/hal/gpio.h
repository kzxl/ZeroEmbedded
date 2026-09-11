#ifndef ZERO_HAL_GPIO_H
#define ZERO_HAL_GPIO_H

/**
 * @file gpio.h
 * @brief Type-safe zero-cost GPIO abstraction for ZeroEmbedded.
 */

#include "../types.h"
#include "../result.h"
#include "../attributes.h"

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

/** Packed 16-bit type-safe pin descriptor (Zero-allocation) */
typedef struct {
    uint8_t port : 4;
    uint8_t pin  : 4;
    uint8_t mode : 4;
    uint8_t pull : 2;
    uint8_t speed: 2;
} fw_gpio_t;

#define FW_GPIO_PIN(port_id, pin_id) \
    { (uint8_t)(port_id), (uint8_t)(pin_id), FW_GPIO_MODE_OUTPUT_PP, FW_GPIO_PULL_NONE, FW_GPIO_SPEED_HIGH }

/** Common hardware-level function prototypes implemented by silicon BSP */
fw_status_t fw_gpio_init(fw_gpio_t pin);
void fw_gpio_write(fw_gpio_t pin, fw_bool_t state);
fw_bool_t fw_gpio_read(fw_gpio_t pin);
void fw_gpio_toggle(fw_gpio_t pin);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_GPIO_H */
