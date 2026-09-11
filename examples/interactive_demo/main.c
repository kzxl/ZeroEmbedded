/**
 * @file main.c
 * @brief ZeroEmbedded Real-Time Industrial Node Console Demo
 * Demonstrates a live, interactive, deterministic embedded runtime running
 * with real-time superloop, UART SPSC stream, zero-heap memory pool,
 * sliding-window ZeroWire framing, and cooperative tasklets.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
#include <conio.h>
#include <windows.h>
#endif

#include "zero/zero.h"

/* ========================================================================== */
/* Static Memory Layout (Zero Dynamic Heap / No malloc)                       */
/* ========================================================================== */

#define POOL_BLOCKS      16
#define POOL_BLOCK_SIZE  64
FW_ALIGNED(8) static uint8_t s_pool_memory[POOL_BLOCKS * POOL_BLOCK_SIZE];
static fw_pool_t s_telemetry_pool;

#define UART_BUFFER_SIZE 256
static uint8_t s_uart_tx_raw[UART_BUFFER_SIZE];
static uint8_t s_uart_rx_raw[UART_BUFFER_SIZE];
static fw_spsc_t s_uart_tx_q;
static fw_spsc_t s_uart_rx_q;
static fw_uart_t s_device_uart;

#define TASKLET_CAPACITY 32
static fw_tasklet_item_t s_tasklet_items[TASKLET_CAPACITY];
static fw_tasklet_queue_t s_tasklet_q;

/* Hardware I/O Pin */
static const fw_gpio_t LED_HEARTBEAT = FW_GPIO_PIN(FW_GPIO_PORT_A, FW_GPIO_PIN_5);

/* Protocols & Message IDs */
#define MSG_CMD_PING         0x10
#define MSG_CMD_SET_LED      0x11
#define MSG_TELEMETRY_DATA   0x20
#define MSG_EMERGENCY_ALERT  0x99

/* Runtime Statistics */
typedef struct {
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t crc_errors;
    uint32_t tasklets_executed;
    uint32_t superloop_iterations;
    uint32_t loops_per_sec;
    fw_bool_t auto_heartbeat_enabled;
} node_stats_t;

static node_stats_t g_stats = {0, 0, 0, 0, 0, 0, FW_TRUE};

/* External mock injection helper from uart.c */
extern void fw_uart_mock_inject_rx(fw_uart_t *uart, uint8_t byte);
extern fw_bool_t fw_uart_mock_extract_tx(fw_uart_t *uart, uint8_t *out_byte);

/* ========================================================================== */
/* Cooperative Tasklet Handlers (Deferred Execution from ISR)                 */
/* ========================================================================== */

static void on_packet_processed_tasklet(void *context, uint32_t arg) {
    (void)context;
    uint8_t msg_id = (uint8_t)(arg & 0xFF);
    uint8_t seq    = (uint8_t)((arg >> 8) & 0xFF);

    g_stats.tasklets_executed++;

    switch (msg_id) {
        case MSG_CMD_PING:
            printf("  [NODE:TASKLET] Responding to PING command (Seq: %u). Sending PONG.\n", seq);
            break;

        case MSG_CMD_SET_LED: {
            fw_gpio_toggle(LED_HEARTBEAT);
            printf("  [NODE:TASKLET] Remote Host commanded LED toggle -> LED state is now [%s]\n",
                fw_gpio_read(LED_HEARTBEAT) ? "ACTIVE (HIGH)" : "OFF (LOW)");
            break;
        }

        case MSG_TELEMETRY_DATA:
            printf("  [NODE:TASKLET] Telemetry frame #%u verified and processed into memory.\n", seq);
            break;

        case MSG_EMERGENCY_ALERT:
            printf("  \033[1;31m[NODE:ALERT] CRITICAL ALERT EVENT DISPATCHED! (Seq: %u)\033[0m\n", seq);
            break;

        default:
            printf("  [NODE:TASKLET] Processed custom MsgID 0x%02X (Seq: %u)\n", msg_id, seq);
            break;
    }
}

/* ========================================================================== */
/* Firmware Communication Routines                                             */
/* ========================================================================== */

static void send_zerowire_packet(uint8_t msg_id, const void *payload, uint16_t len) {
    fw_zerowire_frame_t frame;
    frame.seq = (uint8_t)(++g_stats.packets_sent & 0xFF);
    frame.msg_id = msg_id;
    frame.length = len;
    if (len > 0 && payload != FW_NULL) {
        memcpy(frame.payload, payload, len);
    }

    uint8_t wire_buf[128];
    fw_span_t out_span = FW_SPAN_FROM_ARRAY(wire_buf);
    fw_size_t encoded = fw_zerowire_encode(&frame, out_span);

    if (encoded > 0) {
        fw_cspan_t tx_cspan = fw_cspan_make(wire_buf, encoded);
        fw_uart_write(&s_device_uart, tx_cspan);
    }
}

/* Simulates incoming transmission wire by draining UART TX into UART RX */
static void service_virtual_wire_loopback(void) {
    uint8_t byte = 0;
    while (fw_uart_mock_extract_tx(&s_device_uart, &byte)) {
        fw_uart_mock_inject_rx(&s_device_uart, byte);
    }
}

/* Reads UART RX stream and decodes ZeroWire binary frames */
static void service_uart_rx_stream(void) {
    uint8_t rx_raw[128];
    fw_span_t rx_span = FW_SPAN_FROM_ARRAY(rx_raw);
    fw_size_t read_bytes = fw_uart_read(&s_device_uart, rx_span);

    if (read_bytes == 0) return;

    fw_cspan_t stream_slice = fw_cspan_make(rx_raw, read_bytes);
    fw_zerowire_frame_t rx_frame;
    fw_size_t consumed = 0;

    fw_status_t st = fw_zerowire_stream_sync(stream_slice, &rx_frame, &consumed);
    if (st == FW_OK) {
        g_stats.packets_received++;
        /* Post tasklet to superloop queue without blocking interrupt or stream */
        uint32_t arg = ((uint32_t)rx_frame.seq << 8) | rx_frame.msg_id;
        fw_tasklet_post(&s_tasklet_q, on_packet_processed_tasklet, FW_NULL, arg);
    } else {
        if (consumed > 0) {
            g_stats.crc_errors++;
            printf("  \033[1;33m[NODE:STREAM] Noise/Corrupted frame detected! Stream resync dropped %zu invalid byte(s).\033[0m\n", consumed);
        }
    }
}

/* Generates periodic sensor telemetry (Temperature, Voltage, Pressure) */
static void emit_sensor_telemetry(void) {
    /* Allocate 1 block from O(1) bitmap memory pool */
    char *telemetry_str = (char*)fw_pool_alloc(&s_telemetry_pool);
    if (telemetry_str == FW_NULL) {
        printf("  [ERROR] Telemetry pool exhausted!\n");
        return;
    }

    float temp = 24.5f + (float)(rand() % 40) * 0.1f;
    float volt = 3.28f + (float)(rand() % 10) * 0.01f;
    uint32_t uptime = fw_timer_get_millis();

    snprintf(telemetry_str, POOL_BLOCK_SIZE, "UP:%ums|TMP:%.1fC|VCC:%.2fV", uptime, temp, volt);

    send_zerowire_packet(MSG_TELEMETRY_DATA, telemetry_str, (uint16_t)strlen(telemetry_str));

    /* Free block immediately back to pool (O(1) bitmap cycle safe) */
    fw_pool_free(&s_telemetry_pool, telemetry_str);
}

/* ========================================================================== */
/* UI & Interactive Console Helpers                                           */
/* ========================================================================== */

static void print_banner(void) {
    printf("\n\033[1;36m====================================================================================\033[0m\n");
    printf("\033[1;32m  ⚡ ZEROEMBEDDED: REAL-TIME INDUSTRIAL EDGE NODE RUNTIME CONSOLE \033[0m\n");
    printf("\033[1;36m====================================================================================\033[0m\n");
    printf(" Architecture: Sovereign Hybrid C Tier | Zero Dynamic Heap | Lock-Free SPSC\n");
    printf(" Memory Pool : %d blocks x %d bytes | Non-blocking Event Superloop Active\n\n",
        POOL_BLOCKS, POOL_BLOCK_SIZE);

    printf("\033[1;33m--- Interactive Command Shortcuts ---\033[0m\n");
    printf("  \033[1;37m[1]\033[0m Toggle Status LED (PA5)\n");
    printf("  \033[1;37m[2]\033[0m Trigger Sensor Telemetry Packet (ZeroWire Frame + CRC16)\n");
    printf("  \033[1;37m[3]\033[0m Simulate Host PING Command Packet\n");
    printf("  \033[1;37m[4]\033[0m Stress-Test Memory Pool (All 16 Blocks Alloc + Free Bench)\n");
    printf("  \033[1;37m[5]\033[0m Inject Line Noise / Corrupted Packet (Test Resync & CRC Reject)\n");
    printf("  \033[1;37m[6]\033[0m Toggle 1-Second Autonomous Heartbeat (Currently: \033[1;32mON\033[0m)\n");
    printf("  \033[1;37m[s]\033[0m Print Full System Diagnostics & Statistics\n");
    printf("  \033[1;37m[q]\033[0m Quit Real-Time Simulation\n");
    printf("\033[1;36m------------------------------------------------------------------------------------\033[0m\n\n");
}

static void print_diagnostics(void) {
    printf("\n\033[1;35m>>> [SYSTEM DIAGNOSTICS SNAPSHOT] <<<\033[0m\n");
    printf("  * System Uptime          : %u ms (%.2f s)\n", fw_timer_get_millis(), (float)fw_timer_get_millis() / 1000.0f);
    printf("  * Superloop Iterations   : %u loops (~%u loops/sec)\n", g_stats.superloop_iterations, g_stats.loops_per_sec);
    printf("  * Memory Pool Capacity   : %zu blocks total (%zu available, %zu in-use)\n",
        fw_pool_capacity(&s_telemetry_pool),
        fw_pool_available(&s_telemetry_pool),
        fw_pool_capacity(&s_telemetry_pool) - fw_pool_available(&s_telemetry_pool));
    printf("  * LED Hardware Status    : Pin PA5 is [%s]\n", fw_gpio_read(LED_HEARTBEAT) ? "ON (HIGH)" : "OFF (LOW)");
    printf("  * ZeroWire Packets TX/RX : TX=%u frames, RX=%u frames\n", g_stats.packets_sent, g_stats.packets_received);
    printf("  * CRC16 Rejections       : %u frames dropped by stream resync\n", g_stats.crc_errors);
    printf("  * Tasklets Executed      : %u deferred work items\n", g_stats.tasklets_executed);
    printf("  * Autonomous 1Hz Stream  : [%s]\n\n", g_stats.auto_heartbeat_enabled ? "ENABLED" : "PAUSED");
}

/* ========================================================================== */
/* Main Real-Time Superloop Entry Point                                       */
/* ========================================================================== */

int main(int argc, char **argv) {
    fw_bool_t auto_mode = FW_FALSE;
    if (argc > 1 && strcmp(argv[1], "--auto") == 0) {
        auto_mode = FW_TRUE;
        printf("\033[1;36m[MODE] Running in Automated Self-Demonstration Mode (--auto)\033[0m\n");
    }

    /* 1. Hardware & Framework Subsystems Initialization */
    fw_gpio_init(LED_HEARTBEAT);
    fw_gpio_write(LED_HEARTBEAT, FW_FALSE);

    fw_pool_init(&s_telemetry_pool, s_pool_memory, sizeof(s_pool_memory), POOL_BLOCK_SIZE, 8);
    fw_spsc_init(&s_uart_tx_q, s_uart_tx_raw, UART_BUFFER_SIZE);
    fw_spsc_init(&s_uart_rx_q, s_uart_rx_raw, UART_BUFFER_SIZE);

    fw_uart_config_t uart_cfg = { 115200, FW_UART_PARITY_NONE, FW_UART_STOP_1, FW_FALSE };
    fw_uart_init(&s_device_uart, (void*)0x40004400, &uart_cfg, &s_uart_tx_q, &s_uart_rx_q);

    fw_tasklet_queue_init(&s_tasklet_q, s_tasklet_items, TASKLET_CAPACITY);

    print_banner();

    /* 2. Setup Non-blocking Timers */
    fw_timeout_t timer_heartbeat;
    fw_timeout_start(&timer_heartbeat, 1000);

    fw_timeout_t timer_1sec_perf;
    fw_timeout_start(&timer_1sec_perf, 1000);
    uint32_t loop_counter = 0;

    uint32_t start_time = fw_timer_get_millis();
    int auto_step = 0;
    fw_bool_t is_running = FW_TRUE;

    /* 3. Non-Blocking Real-Time Execution Loop */
    while (is_running) {
        g_stats.superloop_iterations++;
        loop_counter++;

        /* A. Handle Autonomous 1-Second Sensor Heartbeat */
        if (g_stats.auto_heartbeat_enabled && fw_timeout_is_expired(&timer_heartbeat)) {
            fw_timeout_start(&timer_heartbeat, 1000);
            printf("\033[0;32m[1Hz TICK]\033[0m Autonomous Telemetry Burst: ");
            emit_sensor_telemetry();
            fw_gpio_toggle(LED_HEARTBEAT);
            printf("TX frame #%u dispatched (LED: %s)\n",
                g_stats.packets_sent, fw_gpio_read(LED_HEARTBEAT) ? "ON" : "OFF");
        }

        /* B. Measure Loop Rate per second */
        if (fw_timeout_is_expired(&timer_1sec_perf)) {
            fw_timeout_start(&timer_1sec_perf, 1000);
            g_stats.loops_per_sec = loop_counter;
            loop_counter = 0;
        }

        /* C. Service Communications (TX loopback -> RX stream sync) */
        service_virtual_wire_loopback();
        service_uart_rx_stream();

        /* D. Dispatch pending tasklets from ISR queue */
        fw_tasklet_dispatch_all(&s_tasklet_q);

        /* E1. Automated Demo Sequence (when run with --auto) */
        if (auto_mode) {
            uint32_t elapsed = fw_timer_get_millis() - start_time;
            if (auto_step == 0 && elapsed >= 400) {
                auto_step = 1;
                fw_gpio_toggle(LED_HEARTBEAT);
                printf("\033[1;33m[AUTO ACTION 1 @ %ums]\033[0m Toggle LED PA5 -> State: \033[1;32m[%s]\033[0m\n",
                    elapsed, fw_gpio_read(LED_HEARTBEAT) ? "ON" : "OFF");
            } else if (auto_step == 1 && elapsed >= 800) {
                auto_step = 2;
                printf("\033[1;33m[AUTO ACTION 2 @ %ums]\033[0m Triggering manual telemetry packet...\n", elapsed);
                emit_sensor_telemetry();
            } else if (auto_step == 2 && elapsed >= 1200) {
                auto_step = 3;
                printf("\033[1;33m[AUTO ACTION 3 @ %ums]\033[0m Simulating incoming Host PING packet...\n", elapsed);
                send_zerowire_packet(MSG_CMD_PING, "PING_FROM_HOST", 14);
            } else if (auto_step == 3 && elapsed >= 1600) {
                auto_step = 4;
                printf("\033[1;33m[AUTO ACTION 4 @ %ums]\033[0m Stress-testing O(1) Memory Pool alloc/free...\n", elapsed);
                void *blocks[POOL_BLOCKS];
                uint64_t t_start = fw_timer_get_micros();
                for (int i = 0; i < POOL_BLOCKS; ++i) {
                    blocks[i] = fw_pool_alloc(&s_telemetry_pool);
                }
                for (int i = 0; i < POOL_BLOCKS; ++i) {
                    fw_pool_free(&s_telemetry_pool, blocks[i]);
                }
                uint64_t elapsed_us = fw_timer_get_micros() - t_start;
                printf("  \033[1;32m-> Allocated & Freed %d blocks in %llu us (%.2f ns/op). Available: %zu blocks.\033[0m\n",
                    POOL_BLOCKS, (unsigned long long)elapsed_us, (double)elapsed_us * 1000.0 / (POOL_BLOCKS * 2),
                    fw_pool_available(&s_telemetry_pool));
            } else if (auto_step == 4 && elapsed >= 2000) {
                auto_step = 5;
                printf("\033[1;33m[AUTO ACTION 5 @ %ums]\033[0m Injecting line noise + corrupted packet...\n", elapsed);
                uint8_t garbage[12] = { 0xAA, 0x55, 0x01, 0x99, 0x02, 0x00, 0xDE, 0xAD, 0x00, 0x00 };
                fw_cspan_t noise_span = FW_CSPAN_FROM_ARRAY(garbage);
                fw_uart_write(&s_device_uart, noise_span);
            } else if (auto_step == 5 && elapsed >= 2400) {
                auto_step = 6;
                print_diagnostics();
            } else if (auto_step == 6 && elapsed >= 2800) {
                printf("\n\033[1;32m[AUTO DEMO] Automated demonstration completed successfully! Exiting...\033[0m\n");
                is_running = FW_FALSE;
            }
        }

        /* E2. Non-blocking Interactive User Keyboard Input */
#if defined(_WIN32) || defined(_WIN64)
        if (_kbhit()) {
            int ch = _getch();
            switch (ch) {
                case '1': {
                    fw_gpio_toggle(LED_HEARTBEAT);
                    printf("\033[1;33m[USER ACTION 1]\033[0m Manually toggled LED PA5 -> State: \033[1;32m[%s]\033[0m\n",
                        fw_gpio_read(LED_HEARTBEAT) ? "ON" : "OFF");
                    break;
                }

                case '2': {
                    printf("\033[1;33m[USER ACTION 2]\033[0m Triggering manual telemetry packet...\n");
                    emit_sensor_telemetry();
                    break;
                }

                case '3': {
                    printf("\033[1;33m[USER ACTION 3]\033[0m Simulating incoming Host PING packet...\n");
                    send_zerowire_packet(MSG_CMD_PING, "PING_FROM_HOST", 14);
                    break;
                }

                case '4': {
                    printf("\033[1;33m[USER ACTION 4]\033[0m Stress-testing O(1) Memory Pool alloc/free...\n");
                    void *blocks[POOL_BLOCKS];
                    uint64_t t_start = fw_timer_get_micros();
                    for (int i = 0; i < POOL_BLOCKS; ++i) {
                        blocks[i] = fw_pool_alloc(&s_telemetry_pool);
                    }
                    for (int i = 0; i < POOL_BLOCKS; ++i) {
                        fw_pool_free(&s_telemetry_pool, blocks[i]);
                    }
                    uint64_t elapsed_us = fw_timer_get_micros() - t_start;
                    printf("  \033[1;32m-> Allocated & Freed %d blocks in %llu us (%.2f ns/op). Available: %zu blocks.\033[0m\n",
                        POOL_BLOCKS, (unsigned long long)elapsed_us, (double)elapsed_us * 1000.0 / (POOL_BLOCKS * 2),
                        fw_pool_available(&s_telemetry_pool));
                    break;
                }

                case '5': {
                    printf("\033[1;33m[USER ACTION 5]\033[0m Injecting 8 noise bytes + corrupted packet...\n");
                    uint8_t garbage[12] = { 0xAA, 0x55, 0x01, 0x99, 0x02, 0x00, 0xDE, 0xAD, 0x00, 0x00 };
                    fw_cspan_t noise_span = FW_CSPAN_FROM_ARRAY(garbage);
                    fw_uart_write(&s_device_uart, noise_span);
                    break;
                }

                case '6': {
                    g_stats.auto_heartbeat_enabled = !g_stats.auto_heartbeat_enabled;
                    printf("\033[1;33m[USER ACTION 6]\033[0m Autonomous 1Hz heartbeat is now \033[1;%sm[%s]\033[0m\n",
                        g_stats.auto_heartbeat_enabled ? "32" : "31",
                        g_stats.auto_heartbeat_enabled ? "ENABLED" : "PAUSED");
                    break;
                }

                case 's':
                case 'S':
                    print_diagnostics();
                    break;

                case 'q':
                case 'Q':
                case 27: /* ESC */
                    printf("\n\033[1;31m[SHUTDOWN] Exiting ZeroEmbedded interactive console demo...\033[0m\n");
                    is_running = FW_FALSE;
                    break;

                default:
                    printf("  [INFO] Unknown shortcut '%c'. Press [s] for stats, [q] to quit.\n", ch);
                    break;
            }
        }
#endif

        /* F. Small cooperative yield sleep (1ms) to keep host CPU usage low */
        fw_delay_millis(1);
    }

    print_diagnostics();
    printf("[DONE] Firmware runtime terminated cleanly. All resources statically preserved.\n\n");
    return 0;
}
