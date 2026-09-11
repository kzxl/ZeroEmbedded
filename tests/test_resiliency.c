/**
 * @file test_resiliency.c
 * @brief Unit tests for Crash Dump Capturer and Dual-Bank OTA Lifecycle
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zero/zero.h"

/* ========================================================================== */
/* 1. Crash Dump & Fault Diagnostics Tests                                    */
/* ========================================================================== */

static uint8_t s_mock_retention_ram[512];

static void test_crash_dump_diagnostics(void) {
    printf("[TEST] Crash Dump: Register capture, CRC verification, and fault report...\n");

    /* 1. Initialize with uninitialized retention memory */
    memset(s_mock_retention_ram, 0, sizeof(s_mock_retention_ram));
    fw_crash_dump_init(s_mock_retention_ram, sizeof(s_mock_retention_ram));
    assert(fw_crash_dump_has_valid() == FW_FALSE);

    /* 2. Populate mock HardFault event caused by Divide-by-Zero */
    fw_crash_dump_t fault_snapshot;
    memset(&fault_snapshot, 0, sizeof(fault_snapshot));
    fault_snapshot.pc           = 0x08001A42;
    fault_snapshot.lr           = 0x08000F20;
    fault_snapshot.sp           = 0x20002000;
    fault_snapshot.r0           = 0x0000000A;
    fault_snapshot.r1           = 0x00000000; /* Divisor was 0 */
    fault_snapshot.xpsr         = 0x61000000;
    fault_snapshot.cfsr         = FW_CFSR_DIVBYZERO;
    fault_snapshot.hfsr         = 0x40000000; /* FORCED */
    fault_snapshot.timestamp_ms = 45678;

    assert(fw_crash_dump_capture(&fault_snapshot) == FW_OK);
    assert(fw_crash_dump_has_valid() == FW_TRUE);

    /* 3. Retrieve captured dump */
    fw_crash_dump_t retrieved;
    assert(fw_crash_dump_get(&retrieved) == FW_OK);
    assert(retrieved.pc == 0x08001A42);
    assert(retrieved.cfsr == FW_CFSR_DIVBYZERO);
    assert(retrieved.timestamp_ms == 45678);

    /* 4. Format human-readable diagnostic report */
    char report[512];
    fw_size_t written = fw_crash_dump_format_report(&retrieved, report, sizeof(report));
    assert(written > 0);
    assert(strstr(report, "Divide-by-Zero") != FW_NULL);
    assert(strstr(report, "0x08001A42") != FW_NULL);
    assert(strstr(report, "45678 ms") != FW_NULL);

    /* 5. Verify corruption check */
    s_mock_retention_ram[10] ^= 0xFF; /* Corrupt data */
    assert(fw_crash_dump_has_valid() == FW_FALSE);

    /* 6. Clear dump */
    fw_crash_dump_clear();
    assert(fw_crash_dump_has_valid() == FW_FALSE);

    printf("  -> Crash dump diagnostics passed! (Report decoded accurately)\n");
}

/* ========================================================================== */
/* 2. Dual-Bank Bootloader & OTA Lifecycle Tests                              */
/* ========================================================================== */

#define OTA_SECTOR_SIZE 512
#define OTA_TOTAL_FLASH (OTA_SECTOR_SIZE * 4)

static uint8_t s_ota_flash[OTA_TOTAL_FLASH];

static fw_status_t ota_mock_read(void *ctx, uint32_t addr, void *buf, fw_size_t len) {
    (void)ctx;
    if (addr + len > OTA_TOTAL_FLASH) return FW_ERR_INVALID_ARG;
    memcpy(buf, &s_ota_flash[addr], len);
    return FW_OK;
}

static fw_status_t ota_mock_write(void *ctx, uint32_t addr, const void *buf, fw_size_t len) {
    (void)ctx;
    if (addr + len > OTA_TOTAL_FLASH) return FW_ERR_INVALID_ARG;
    const uint8_t *src = (const uint8_t*)buf;
    for (fw_size_t i = 0; i < len; ++i) {
        uint8_t cur = s_ota_flash[addr + i];
        uint8_t des = src[i];
        if ((cur & des) != des) return FW_ERR_IO;
        s_ota_flash[addr + i] = cur & des;
    }
    return FW_OK;
}

static fw_status_t ota_mock_erase(void *ctx, uint32_t sector_addr) {
    (void)ctx;
    if (sector_addr + OTA_SECTOR_SIZE > OTA_TOTAL_FLASH || (sector_addr % OTA_SECTOR_SIZE) != 0) {
        return FW_ERR_INVALID_ARG;
    }
    memset(&s_ota_flash[sector_addr], 0xFF, OTA_SECTOR_SIZE);
    return FW_OK;
}

static const fw_flash_driver_t s_ota_flash_drv = {
    OTA_SECTOR_SIZE,
    ota_mock_read,
    ota_mock_write,
    ota_mock_erase,
    FW_NULL
};

static void test_ota_lifecycle(void) {
    printf("[TEST] OTA Lifecycle: 5-stage state machine, image CRC32, and rollback...\n");

    /* Partition layout:
     * 0x0000: Bootloader Metadata Sector
     * 0x0200: Slot 1 (Bank 1 Active)
     * 0x0400: Slot 2 (Bank 2 Staging) */
    memset(s_ota_flash, 0xFF, sizeof(s_ota_flash));

    fw_ota_t ota;
    assert(fw_ota_init(&ota, &s_ota_flash_drv, 0x0000) == FW_OK);

    fw_app_desc_t desc;
    assert(fw_ota_get_desc(&ota, &desc) == FW_OK);
    assert(desc.state == FW_OTA_STATE_EMPTY);

    /* 1. Simulate downloading a new firmware binary (128 bytes) into Staging Slot (0x0400) */
    uint8_t new_firmware[128];
    for (int i = 0; i < 128; ++i) {
        new_firmware[i] = (uint8_t)(i ^ 0x5A);
    }
    uint32_t expected_crc = fw_crc32_compute(new_firmware, sizeof(new_firmware));

    assert(s_ota_flash_drv.write(FW_NULL, 0x0400, new_firmware, sizeof(new_firmware)) == FW_OK);

    /* 2. Create descriptor for new firmware */
    fw_app_desc_t new_desc;
    memset(&new_desc, 0, sizeof(new_desc));
    new_desc.magic         = FW_OTA_DESC_MAGIC;
    new_desc.version_major = 2;
    new_desc.version_minor = 0;
    new_desc.version_patch = 0;
    new_desc.image_size    = sizeof(new_firmware);
    new_desc.image_crc32   = expected_crc;
    new_desc.state         = FW_OTA_STATE_DOWNLOADED;
    new_desc.max_attempts  = 3;

    assert(fw_ota_set_desc(&ota, &new_desc) == FW_OK);

    /* 3. Verify firmware image integrity across Flash */
    assert(fw_ota_verify_image(&s_ota_flash_drv, 0x0400, &new_desc) == FW_OK);

    /* 4. Bootloader marks PENDING_VERIFY before jumping into new image */
    assert(fw_ota_mark_pending_verify(&ota) == FW_OK);
    assert(fw_ota_get_desc(&ota, &desc) == FW_OK);
    assert(desc.state == FW_OTA_STATE_PENDING_VERIFY);
    assert(desc.boot_attempts == 1);

    /* 5. Application boots, runs self-test, and confirms itself */
    assert(fw_ota_confirm(&ota) == FW_OK);
    assert(fw_ota_get_desc(&ota, &desc) == FW_OK);
    assert(desc.state == FW_OTA_STATE_CONFIRMED);
    assert(desc.boot_attempts == 0);

    /* 6. Test Rollback trigger */
    assert(fw_ota_rollback(&ota) == FW_OK);
    assert(fw_ota_get_desc(&ota, &desc) == FW_OK);
    assert(desc.state == FW_OTA_STATE_ROLLBACK);

    /* 7. Corrupted image rejection test */
    s_ota_flash[0x0400 + 10] ^= 0x01; /* Corrupt 1 byte in flash */
    assert(fw_ota_verify_image(&s_ota_flash_drv, 0x0400, &new_desc) == FW_ERR_CORRUPTED);

    printf("  -> OTA lifecycle passed! (States: DOWNLOADED -> PENDING -> CONFIRMED -> ROLLBACK)\n");
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void) {
    printf("\n====================================================================================\n");
    printf("   ZEROEMBEDDED RESILIENCY TEST SUITE (Crash Dump & Dual-Bank OTA Lifecycle)\n");
    printf("====================================================================================\n\n");

    test_crash_dump_diagnostics();
    test_ota_lifecycle();

    printf("\n\033[1;32m[PASS] ALL RESILIENCY & OTA TESTS PASSED SUCCESSFULLY!\033[0m\n\n");
    return 0;
}
