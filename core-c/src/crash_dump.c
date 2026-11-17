/**
 * @file crash_dump.c
 * @brief CPU HardFault & Crash Dump Capturer implementation
 */

#include "zero/hal/crash_dump.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static fw_crash_dump_t s_default_retention_buffer;
static fw_crash_dump_t *s_retention_ptr = &s_default_retention_buffer;

static uint32_t calculate_checksum(const fw_crash_dump_t *dump) {
    const uint32_t *words = (const uint32_t*)dump;
    fw_size_t word_count = (sizeof(fw_crash_dump_t) - sizeof(dump->checksum)) / sizeof(uint32_t);
    uint32_t sum = 0xA5A55A5AU;

    for (fw_size_t i = 0; i < word_count; ++i) {
        sum = (sum << 1) ^ words[i];
    }
    return sum;
}

void fw_crash_dump_init(void *retention_buf, fw_size_t buf_size) {
    if (retention_buf != FW_NULL && buf_size >= sizeof(fw_crash_dump_t)) {
        s_retention_ptr = (fw_crash_dump_t*)retention_buf;
    } else {
        s_retention_ptr = &s_default_retention_buffer;
    }
}

fw_status_t fw_crash_dump_capture(const fw_crash_dump_t *dump) {
    if (dump == FW_NULL || s_retention_ptr == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    /* Direct copy into uninitialized retention memory */
    memcpy(s_retention_ptr, dump, sizeof(fw_crash_dump_t));
    s_retention_ptr->magic    = FW_CRASH_DUMP_MAGIC;
    s_retention_ptr->checksum = calculate_checksum(s_retention_ptr);

    return FW_OK;
}

fw_bool_t fw_crash_dump_has_valid(void) {
    if (s_retention_ptr == FW_NULL) {
        return FW_FALSE;
    }

    if (s_retention_ptr->magic != FW_CRASH_DUMP_MAGIC) {
        return FW_FALSE;
    }

    uint32_t expected = calculate_checksum(s_retention_ptr);
    return (s_retention_ptr->checksum == expected) ? FW_TRUE : FW_FALSE;
}

fw_status_t fw_crash_dump_get(fw_crash_dump_t *out_dump) {
    if (out_dump == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    if (!fw_crash_dump_has_valid()) {
        return FW_ERR_NOT_FOUND;
    }

    memcpy(out_dump, s_retention_ptr, sizeof(fw_crash_dump_t));
    return FW_OK;
}

void fw_crash_dump_clear(void) {
    if (s_retention_ptr != FW_NULL) {
        memset(s_retention_ptr, 0, sizeof(fw_crash_dump_t));
    }
}

static fw_size_t append_cause_str(char *buf, fw_size_t cap, fw_size_t len, const char *text) {
    if (len + 1U >= cap) {
        return len;
    }
    int n = snprintf(buf + len, cap - len, "%s", text);
    if (n <= 0) {
        return len;
    }
    fw_size_t avail = cap - len - 1U;
    return len + (((fw_size_t)n > avail) ? avail : (fw_size_t)n);
}

static fw_size_t append_cause_addr(
    char *buf,
    fw_size_t cap,
    fw_size_t len,
    const char *prefix,
    uint32_t addr,
    const char *suffix
) {
    if (len + 1U >= cap) {
        return len;
    }
    int n = snprintf(buf + len, cap - len, "%s0x%08" PRIX32 "%s", prefix, addr, suffix);
    if (n <= 0) {
        return len;
    }
    fw_size_t avail = cap - len - 1U;
    return len + (((fw_size_t)n > avail) ? avail : (fw_size_t)n);
}

fw_size_t fw_crash_dump_format_report(const fw_crash_dump_t *dump, char *out_str, fw_size_t max_len) {
    if (dump == FW_NULL || out_str == FW_NULL || max_len == 0) {
        return 0;
    }

    char fault_cause[256] = { 0 };
    fw_size_t cause_len = 0;

    if (dump->cfsr & FW_CFSR_DIVBYZERO) {
        cause_len = append_cause_str(fault_cause, sizeof(fault_cause), cause_len, " [UsageFault: Divide-by-Zero]");
    }
    if (dump->cfsr & FW_CFSR_UNALIGN_TRP) {
        cause_len = append_cause_str(fault_cause, sizeof(fault_cause), cause_len, " [UsageFault: Unaligned Access]");
    }
    if (dump->cfsr & FW_CFSR_UNDEFINSTR) {
        cause_len = append_cause_str(fault_cause, sizeof(fault_cause), cause_len, " [UsageFault: Undefined Instruction]");
    }
    if (dump->cfsr & FW_CFSR_PRECISERR) {
        if (dump->cfsr & FW_CFSR_BFARVALID) {
            cause_len = append_cause_addr(fault_cause, sizeof(fault_cause), cause_len,
                " [BusFault: Precise Data Access Error at ", dump->bfar, "]");
        } else {
            cause_len = append_cause_str(fault_cause, sizeof(fault_cause), cause_len,
                " [BusFault: Precise Data Access Error]");
        }
    }
    if (dump->cfsr & FW_CFSR_DACCVIOL) {
        if (dump->cfsr & FW_CFSR_MMARVALID) {
            cause_len = append_cause_addr(fault_cause, sizeof(fault_cause), cause_len,
                " [MemManage: Data Access Violation at ", dump->mmar, "]");
        } else {
            cause_len = append_cause_str(fault_cause, sizeof(fault_cause), cause_len,
                " [MemManage: Data Access Violation]");
        }
    }
    if (dump->cfsr & FW_CFSR_IACCVIOL) {
        cause_len = append_cause_str(fault_cause, sizeof(fault_cause), cause_len,
            " [MemManage: Instruction Access Violation]");
    }

    if (cause_len == 0) {
        snprintf(fault_cause, sizeof(fault_cause),
            " [Forced HardFault / Unknown (CFSR: 0x%08" PRIX32 ")]", dump->cfsr);
    }

    int written = snprintf(out_str, max_len,
        "=== HARDFAULT CRASH DUMP ===\n"
        "  * Fault Root Cause: %s\n"
        "  * Program Counter : 0x%08" PRIX32 " (PC)\n"
        "  * Link Register   : 0x%08" PRIX32 " (LR)\n"
        "  * Stack Pointer   : 0x%08" PRIX32 " (SP)\n"
        "  * Status (xPSR)   : 0x%08" PRIX32 "\n"
        "  * Registers R0..R3: [0x%08" PRIX32 ", 0x%08" PRIX32 ", 0x%08" PRIX32 ", 0x%08" PRIX32 "]\n"
        "  * Register R12    : 0x%08" PRIX32 "\n"
        "  * HFSR / CFSR     : HFSR=0x%08" PRIX32 ", CFSR=0x%08" PRIX32 "\n"
        "  * Uptime at Crash : %" PRIu32 " ms\n"
        "============================",
        fault_cause, dump->pc, dump->lr, dump->sp, dump->xpsr,
        dump->r0, dump->r1, dump->r2, dump->r3, dump->r12,
        dump->hfsr, dump->cfsr, dump->timestamp_ms);

    if (written < 0) {
        return 0;
    }
    return ((fw_size_t)written < max_len) ? (fw_size_t)written : (max_len - 1);
}
