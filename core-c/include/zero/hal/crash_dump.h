#ifndef ZERO_HAL_CRASH_DUMP_H
#define ZERO_HAL_CRASH_DUMP_H

/**
 * @file crash_dump.h
 * @brief CPU HardFault & Crash Dump Capturer for Retention RAM (.noinit)
 * Captures core registers (PC, LR, SP, xPSR) and fault status registers (CFSR, HFSR, MMAR, BFAR)
 * to preserve field diagnostics across soft/watchdog resets without requiring a debugger.
 */

#include "zero/types.h"
#include "zero/result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_CRASH_DUMP_MAGIC 0x43525348U /* 'CRSH' */

/* Configurable Fault Status Register (CFSR) bit masks (ARM Cortex-M standard) */
#define FW_CFSR_DIVBYZERO    (1U << 25) /* Divide by zero attempted */
#define FW_CFSR_UNALIGN_TRP  (1U << 24) /* Unaligned memory access attempted */
#define FW_CFSR_NOCP         (1U << 19) /* Coprocessor / FPU disabled access */
#define FW_CFSR_INVPC        (1U << 18) /* Invalid EXC_RETURN loaded to PC */
#define FW_CFSR_INVSTATE     (1U << 17) /* Invalid execution state (e.g. non-Thumb bit) */
#define FW_CFSR_UNDEFINSTR   (1U << 16) /* Undefined instruction executed */
#define FW_CFSR_BFARVALID    (1U << 15) /* BFAR holds valid fault address */
#define FW_CFSR_PRECISERR    (1U << 9)  /* Precise data bus access fault */
#define FW_CFSR_IMPRECISERR  (1U << 10) /* Imprecise data bus fault */
#define FW_CFSR_IBUSERR      (1U << 8)  /* Instruction fetch bus error */
#define FW_CFSR_MMARVALID    (1U << 7)  /* MMFAR holds valid fault address */
#define FW_CFSR_IACCVIOL     (1U << 1)  /* Instruction access MPU violation */
#define FW_CFSR_DACCVIOL     (1U << 0)  /* Data access MPU violation */

typedef struct {
    uint32_t magic;         /*!< FW_CRASH_DUMP_MAGIC */
    uint32_t timestamp_ms;  /*!< Uptime in ms when crash occurred */
    uint32_t r0;            /*!< General purpose register R0 */
    uint32_t r1;            /*!< General purpose register R1 */
    uint32_t r2;            /*!< General purpose register R2 */
    uint32_t r3;            /*!< General purpose register R3 */
    uint32_t r12;           /*!< Intra-procedure register R12 */
    uint32_t sp;            /*!< Stack Pointer (MSP or PSP) */
    uint32_t lr;            /*!< Link Register (Caller address) */
    uint32_t pc;            /*!< Program Counter (Exact instruction that faulted!) */
    uint32_t xpsr;          /*!< Program Status Register */
    uint32_t cfsr;          /*!< Configurable Fault Status Register */
    uint32_t hfsr;          /*!< HardFault Status Register */
    uint32_t mmar;          /*!< MemManage Fault Address Register */
    uint32_t bfar;          /*!< BusFault Address Register */
    uint32_t checksum;      /*!< Checksum verifying dump validity */
} fw_crash_dump_t;

/**
 * @brief Initializes the crash dump subsystem with a designated retention RAM buffer.
 *
 * @param retention_buf Pointer to uninitialized RAM (.noinit) or static buffer.
 * @param buf_size Size of the buffer (must be at least sizeof(fw_crash_dump_t)).
 */
void fw_crash_dump_init(void *retention_buf, fw_size_t buf_size);

/**
 * @brief Captures a crash dump into retention memory.
 * Designed to be called safely inside HardFault_Handler or Panic traps.
 */
fw_status_t fw_crash_dump_capture(const fw_crash_dump_t *dump);

/**
 * @brief Checks if retention memory contains a valid, verified crash dump from a prior reset.
 */
fw_bool_t fw_crash_dump_has_valid(void);

/**
 * @brief Retrieves the saved crash dump.
 */
fw_status_t fw_crash_dump_get(fw_crash_dump_t *out_dump);

/**
 * @brief Clears the retention memory after the dump has been logged or reported.
 */
void fw_crash_dump_clear(void);

/**
 * @brief Formats the crash dump into a human-readable diagnostic report string.
 * Decodes the root cause (e.g. "Divide by zero", "Unaligned memory access at 0x...").
 *
 * @param dump Pointer to crash dump.
 * @param out_str Output text buffer.
 * @param max_len Maximum length of out_str buffer.
 * @return Number of characters written.
 */
fw_size_t fw_crash_dump_format_report(const fw_crash_dump_t *dump, char *out_str, fw_size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_CRASH_DUMP_H */
