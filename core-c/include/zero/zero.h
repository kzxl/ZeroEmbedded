#ifndef ZERO_H
#define ZERO_H

/**
 * @file zero.h
 * @brief Master umbrella header for ZeroEmbedded Core C Foundation.
 */

#include "attributes.h"
#include "config.h"
#include "types.h"
#include "assert.h"
#include "result.h"
#include "span.h"
#include "string_view.h"
#include "memory/pool.h"
#include "memory/arena.h"
#include "memory/buffer.h"
#include "sync/spsc.h"
#include "tasklet.h"
#include "hal/gpio.h"
#include "hal/uart.h"
#include "hal/timer.h"
#include "hal/power.h"
#include "hal/dwt.h"
#include "hal/watchdog.h"
#include "protocol/zerowire.h"
#include "protocol/dispatcher.h"
#include "fsm/fsm.h"
#include "dsp/filter.h"
#include "rtos/rtos.h"

#define ZERO_EMBEDDED_VERSION_MAJOR 0
#define ZERO_EMBEDDED_VERSION_MINOR 4
#define ZERO_EMBEDDED_VERSION_PATCH 0

#endif /* ZERO_H */
