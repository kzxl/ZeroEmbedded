# 📖 ZeroEmbedded: Comprehensive Developer & User Guide

**ZeroEmbedded** is an enterprise-grade sovereign embedded framework engineered for deterministic real-time microcontrollers (ARM Cortex-M, RISC-V, AVR, and Host Simulators). It enforces **zero dynamic heap allocation**, **zero runtime overhead**, and **compile-time context safety**.

---

## 1. System Invariants & Architectural Rules

1. **Strictly Non-Heap in Core Execution**: Zero usage of `malloc`, `calloc`, `realloc`, or `free`. All storage buffers are allocated statically with compile-time alignment (`FW_ALIGNED`) and deterministic bounds.
2. **Deterministic $O(1)$ Memory Ops**: Memory pools use forward intrusive free-lists with bitmap cycle detection; linear arenas use monotonic offsets with scoped rewind tokens.
3. **Lock-Free Concurrency Across ISR Boundary**: Single-Producer Single-Consumer (`fw_spsc_t`) queues and cooperative tasklet dispatchers (`fw_tasklet_queue_t`) connect interrupt handlers to the background superloop without global interrupt disables or blocking mutexes.
4. **Compile-Time & CI Safety Discipline**: Interrupt routines annotated with `FW_ISR` must pass `zero_analyzer.py` static checks (forbidding blocking delays, heap allocations, and standard I/O).

---

## 2. Integration into Firmware Projects

### Option A: CMake Integration (Recommended for VS Code, CLion, Zephyr, and CI)

Add ZeroEmbedded to your project repository (as a submodule or subdirectory):

```cmake
cmake_minimum_required(VERSION 3.15)
project(my_firmware LANGUAGES C)

# 1. Add ZeroEmbedded C Core Foundation
add_subdirectory(path/to/ZeroEmbedded/core-c zero_embedded)

# 2. Link against your firmware binary
add_executable(my_firmware src/main.c)
target_link_libraries(my_firmware PRIVATE Zero::core_c)
```

### Option B: Drop-In Source Integration (Keil MDK, STM32CubeIDE, IAR, PlatformIO)

1. **Include Search Path**: Add `core-c/include` to your IDE's compiler include directories.
2. **Source Files**: Add the following files from `core-c/src/` to your project build group:
   - Memory primitives: `pool.c`, `arena.c`, `buffer.c`, `assert.c`
   - Concurrency & Queue: `spsc.c`, `tasklet.c`
   - Protocol framing: `zerowire.c`
   - Hardware Abstraction: `gpio.c`, `timer.c`, `uart.c`
   - RTOS fallback: `rtos_baremetal.c` (if running bare-metal without FreeRTOS)
3. **Master Header**: Include the umbrella header anywhere in your C code:
   ```c
   #include "zero/zero.h"
   ```

---

## 3. Core Modules & Usage Recipes

### 3.1 Fixed-Size Memory Pool (`fw_pool_t`)
Used for deterministic allocation of uniform packets, telemetry objects, or state machines. Protected by an internal bitmap tracker that mathematically prevents double-free vulnerabilities.

```c
#include "zero/zero.h"

#define TELEMETRY_BLOCKS 8
#define TELEMETRY_SIZE   64

/* 1. Statically allocate backing memory with 8-byte alignment */
FW_ALIGNED(8) static uint8_t s_pool_memory[TELEMETRY_BLOCKS * TELEMETRY_SIZE];
static fw_pool_t s_pool;

void init_memory(void) {
    /* Initializes pool in O(N) time at startup */
    fw_pool_init(&s_pool, s_pool_memory, sizeof(s_pool_memory), TELEMETRY_SIZE, 8);
}

void process_data(void) {
    /* 2. O(1) Allocation (Latency: ~5 nanoseconds) */
    void *block = fw_pool_alloc(&s_pool);
    if (block == FW_NULL) {
        // Pool is exhausted (deterministic backpressure)
        return;
    }

    // Work with block...
    memset(block, 0x55, TELEMETRY_SIZE);

    /* 3. O(1) Deallocation (Guaranteed safe against double-free) */
    fw_status_t st = fw_pool_free(&s_pool, block);
    FW_ASSERT(st == FW_OK);
}
```

---

### 3.2 Linear Scoped Arena (`fw_arena_t`)
Used for frame parsing, temporary string construction, or phase-based batch operations. Allocations take $O(1)$ time and can be rewound to a previous checkpoint.

```c
#include "zero/zero.h"

FW_ALIGNED(8) static uint8_t s_arena_mem[1024];
static fw_arena_t s_arena;

void arena_example(void) {
    fw_arena_init(&s_arena, s_arena_mem, sizeof(s_arena_mem));

    /* Checkpoint current offset */
    fw_arena_mark_t mark = fw_arena_mark(&s_arena);

    /* Allocate variable-length temporary scratch buffers */
    void *temp1 = fw_arena_alloc(&s_arena, 128, 8);
    void *temp2 = fw_arena_alloc_zeroed(&s_arena, 64, 4);

    // Perform computation...

    /* Release all scratch memory back to checkpoint in O(1) time */
    fw_arena_rewind(&s_arena, mark);
}
```

---

### 3.3 Lock-Free ISR-to-Thread Ringbuffer (`fw_spsc_t`)
Safe for bidirectional communication between an interrupt routine (Producer) and the main thread (Consumer) without mutexes or disabling global interrupts.

```c
#include "zero/zero.h"

#define RX_CAPACITY 128 /* Must be a power of 2 */
static uint8_t s_rx_storage[RX_CAPACITY];
static fw_spsc_t s_rx_queue;

void comms_init(void) {
    fw_spsc_init(&s_rx_queue, s_rx_storage, RX_CAPACITY);
}

/* Interrupt Service Routine Context */
FW_ISR void USART1_IRQHandler(void) {
    uint8_t received_byte = 0xAA; /* Hardware register read */
    fw_spsc_push(&s_rx_queue, received_byte);
}

/* Background Superloop Context */
void process_incoming(void) {
    uint8_t byte;
    while (fw_spsc_pop(&s_rx_queue, &byte) == FW_OK) {
        // Process received byte safely in user context
    }
}
```

---

### 3.4 Cooperative Tasklet Queue (`fw_tasklet_queue_t`)
Eliminates long interrupt service times by deferring complex handler execution to the main superloop.

```c
#include "zero/zero.h"

#define TASKLET_CAPACITY 16
static fw_tasklet_item_t s_tasklets[TASKLET_CAPACITY];
static fw_tasklet_queue_t s_tasklet_queue;

void on_button_pressed(void *context, uint32_t pin_id) {
    // Heavy work executed safely in superloop, NOT inside ISR!
    printf("Button pin %u handled in main thread\n", pin_id);
}

/* ISR Context */
FW_ISR void EXTI0_IRQHandler(void) {
    // Post tasklet with zero memory allocation
    fw_tasklet_post(&s_tasklet_queue, on_button_pressed, FW_NULL, 0);
}

/* Superloop Context */
void main_loop(void) {
    while (1) {
        fw_tasklet_dispatch_all(&s_tasklet_queue);
    }
}
```

---

### 3.5 Non-Blocking Software Timers (`fw_timeout_t`)
Replaces blocking `delay_ms()` calls, enabling multi-rate periodic operations (e.g. 100Hz PID loop + 1Hz status LED) inside a cooperative superloop.

```c
#include "zero/zero.h"

static const fw_gpio_t STATUS_LED = FW_GPIO_PIN(FW_GPIO_PORT_A, FW_GPIO_PIN_5);

void superloop(void) {
    fw_timeout_t led_timer;
    fw_timeout_start(&led_timer, 500); // 500ms timeout

    while (1) {
        if (fw_timeout_is_expired(&led_timer)) {
            fw_timeout_start(&led_timer, 500); // Reload timer
            fw_gpio_toggle(STATUS_LED);        // Non-blocking toggle
        }

        // CPU immediately handles other communication tasks!
    }
}
```

---

### 3.6 Binary Framing Engine (`ZeroWire` + Table CRC16)
Deterministic protocol framing for UART/SPI/RS-485 interfaces, featuring ROM-LUT accelerated CRC16-CCITT and automatic sliding-window stream resynchronization over noisy channels.

```c
#include "zero/zero.h"

/* 1. Encoding a Packet */
void send_telemetry(fw_uart_t *uart) {
    fw_zerowire_frame_t tx_frame;
    tx_frame.seq = 1;
    tx_frame.msg_id = 0x20;
    tx_frame.length = 4;
    memcpy(tx_frame.payload, "DATA", 4);

    uint8_t out_raw[64];
    fw_size_t encoded_size = fw_zerowire_encode(&tx_frame, FW_SPAN_FROM_ARRAY(out_raw));

    fw_cspan_t span_to_send = fw_cspan_make(out_raw, encoded_size);
    fw_uart_write(uart, span_to_send);
}

/* 2. Decoding a Stream with Automatic Noise Skipping */
void receive_stream(fw_uart_t *uart) {
    uint8_t rx_buf[128];
    fw_size_t bytes_read = fw_uart_read(uart, FW_SPAN_FROM_ARRAY(rx_buf));
    if (bytes_read == 0) return;

    fw_cspan_t stream_span = fw_cspan_make(rx_buf, bytes_read);
    fw_zerowire_frame_t rx_frame;
    fw_size_t consumed = 0;

    fw_status_t status = fw_zerowire_stream_sync(stream_span, &rx_frame, &consumed);
    if (status == FW_OK) {
        // Valid CRC-verified frame ready for consumption!
    }
}
```

---

## 4. Complete Firmware Application Template

The following template represents a production-ready bare-metal firmware archetype incorporating all subsystems:

```c
#include "zero/zero.h"

/* 1. Static Hardware & Memory Layout */
static const fw_gpio_t LED_HEARTBEAT = FW_GPIO_PIN(FW_GPIO_PORT_A, FW_GPIO_PIN_5);

#define POOL_SIZE 8
FW_ALIGNED(8) static uint8_t s_pool_mem[POOL_SIZE * 64];
static fw_pool_t s_pool;

#define UART_Q_SIZE 128
static uint8_t s_tx_mem[UART_Q_SIZE];
static uint8_t s_rx_mem[UART_Q_SIZE];
static fw_spsc_t s_tx_q, s_rx_q;
static fw_uart_t s_uart;

#define TASKLET_SIZE 16
static fw_tasklet_item_t s_tasklets[TASKLET_SIZE];
static fw_tasklet_queue_t s_tasklet_q;

/* 2. Deferred Tasklet Callback */
static void on_rx_packet(void *ctx, uint32_t msg_id) {
    (void)ctx;
    fw_gpio_toggle(LED_HEARTBEAT);
}

/* 3. Simulated/Hardware ISR */
FW_ISR void USART1_IRQHandler(void) {
    uint8_t byte = 0xAA;
    fw_spsc_push(&s_rx_q, byte);
    fw_tasklet_post(&s_tasklet_q, on_rx_packet, FW_NULL, 0x10);
}

/* 4. Firmware Boot & Initialization */
FW_INIT fw_status_t firmware_init(void) {
    FW_CHECK(fw_gpio_init(LED_HEARTBEAT));
    fw_gpio_write(LED_HEARTBEAT, FW_FALSE);

    FW_CHECK(fw_pool_init(&s_pool, s_pool_mem, sizeof(s_pool_mem), 64, 8));
    FW_CHECK(fw_spsc_init(&s_tx_q, s_tx_mem, UART_Q_SIZE));
    FW_CHECK(fw_spsc_init(&s_rx_q, s_rx_mem, UART_Q_SIZE));

    fw_uart_config_t cfg = { 115200, FW_UART_PARITY_NONE, FW_UART_STOP_1, FW_FALSE };
    FW_CHECK(fw_uart_init(&s_uart, FW_NULL, &cfg, &s_tx_q, &s_rx_q));

    FW_CHECK(fw_tasklet_queue_init(&s_tasklet_q, s_tasklets, TASKLET_SIZE));

    return FW_OK;
}

/* 5. Deterministic Real-Time Superloop */
FW_NORMAL int main(void) {
    if (firmware_init() != FW_OK) {
        while (1) { /* Hard fault trap */ }
    }

    fw_timeout_t heartbeat_timer;
    fw_timeout_start(&heartbeat_timer, 1000); // 1 Hz periodic tick

    while (1) {
        /* A. Process all deferred interrupt tasklets */
        fw_tasklet_dispatch_all(&s_tasklet_q);

        /* B. Autonomous 1 Hz Heartbeat */
        if (fw_timeout_is_expired(&heartbeat_timer)) {
            fw_timeout_start(&heartbeat_timer, 1000);
            fw_gpio_toggle(LED_HEARTBEAT);
        }

        /* C. Cooperative idle yield (Host simulation: 1ms sleep) */
        fw_delay_millis(1);
    }

    return 0;
}
```

---

## 5. Verification & Tooling Execution

| Objective | Command | Purpose |
| :--- | :--- | :--- |
| **One-Click Build & Test** | `powershell -File scripts/build_and_test.ps1` | Compiles core, runs CTest suite, and validates host simulation |
| **Interactive Console Demo** | `powershell -File scripts/run_demo.ps1` | Live interactive terminal with hotkeys for testing runtime |
| **Static Context Analyzer** | `python tooling/analyzer/zero_analyzer.py core-c examples` | Enforces zero heap / non-blocking rules inside `FW_ISR` routines |
| **Performance Benchmark** | `.\build\Release\bench_suite.exe` | Validates zero-cost abstraction overhead with HEPM provenance |
