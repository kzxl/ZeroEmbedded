/**
 * @file test_storage.c
 * @brief Unit tests for Wear-Leveling Dual-Bank Ping-Pong Flash Storage (NVS)
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zero/zero.h"

/* ========================================================================== */
/* Virtual Hardware Flash Driver (Simulates Real NOR Flash Behavior)           */
/* ========================================================================== */

#define TEST_SECTOR_SIZE 512
#define TOTAL_FLASH_SIZE (TEST_SECTOR_SIZE * 2)

static uint8_t s_virtual_flash[TOTAL_FLASH_SIZE];

static fw_status_t mock_flash_read(void *ctx, uint32_t addr, void *buf, fw_size_t len) {
    (void)ctx;
    if (addr + len > TOTAL_FLASH_SIZE) {
        return FW_ERR_INVALID_ARG;
    }
    memcpy(buf, &s_virtual_flash[addr], len);
    return FW_OK;
}

static fw_status_t mock_flash_write(void *ctx, uint32_t addr, const void *buf, fw_size_t len) {
    (void)ctx;
    if (addr + len > TOTAL_FLASH_SIZE) {
        return FW_ERR_INVALID_ARG;
    }

    const uint8_t *src = (const uint8_t*)buf;
    for (fw_size_t i = 0; i < len; ++i) {
        /* Real NOR Flash hardware rule: can only flip 1s to 0s, never 0s to 1s without erase! */
        uint8_t current = s_virtual_flash[addr + i];
        uint8_t desired = src[i];
        if ((current & desired) != desired) {
            /* Attempted to program 0 to 1 without erase! */
            return FW_ERR_IO;
        }
        s_virtual_flash[addr + i] = current & desired;
    }
    return FW_OK;
}

static fw_status_t mock_flash_erase(void *ctx, uint32_t sector_addr) {
    (void)ctx;
    if (sector_addr + TEST_SECTOR_SIZE > TOTAL_FLASH_SIZE || (sector_addr % TEST_SECTOR_SIZE) != 0) {
        return FW_ERR_INVALID_ARG;
    }
    /* Erase resets all memory bits to 0xFF */
    memset(&s_virtual_flash[sector_addr], 0xFF, TEST_SECTOR_SIZE);
    return FW_OK;
}

static const fw_flash_driver_t s_mock_flash = {
    TEST_SECTOR_SIZE,
    mock_flash_read,
    mock_flash_write,
    mock_flash_erase,
    FW_NULL
};

/* ========================================================================== */
/* Test Cases                                                                 */
/* ========================================================================== */

static void test_nvs_basic_crud(void) {
    printf("[TEST] NVS: Basic CRUD operations (Create, Read, Update, Delete)...\n");

    /* Initially unformatted flash */
    memset(s_virtual_flash, 0xFF, sizeof(s_virtual_flash));

    fw_nvs_t nvs;
    fw_status_t st = fw_nvs_init(&nvs, &s_mock_flash, 0, TEST_SECTOR_SIZE);
    assert(st == FW_OK);
    assert(nvs.active_bank == 0);
    assert(nvs.sequence == 1);

    /* 1. Write Key 0x1001 */
    const char *serial = "SN-2026-X88";
    st = fw_nvs_write(&nvs, 0x1001, fw_cspan_make(serial, strlen(serial)));
    assert(st == FW_OK);

    /* 2. Read Key 0x1001 */
    char read_buf[32];
    fw_size_t read_len = 0;
    st = fw_nvs_read(&nvs, 0x1001, fw_span_make(read_buf, sizeof(read_buf)), &read_len);
    assert(st == FW_OK);
    assert(read_len == strlen(serial));
    assert(memcmp(read_buf, serial, read_len) == 0);

    /* 3. Update Key 0x1001 with new value (append new record) */
    const char *new_serial = "SN-2026-X99-PRO";
    st = fw_nvs_write(&nvs, 0x1001, fw_cspan_make(new_serial, strlen(new_serial)));
    assert(st == FW_OK);

    memset(read_buf, 0, sizeof(read_buf));
    st = fw_nvs_read(&nvs, 0x1001, fw_span_make(read_buf, sizeof(read_buf)), &read_len);
    assert(st == FW_OK);
    assert(read_len == strlen(new_serial));
    assert(memcmp(read_buf, new_serial, read_len) == 0);

    /* 4. Write Key 0x2002 (Binary data) */
    uint32_t calib_val = 0x12345678;
    st = fw_nvs_write(&nvs, 0x2002, fw_cspan_make(&calib_val, sizeof(calib_val)));
    assert(st == FW_OK);

    uint32_t read_calib = 0;
    st = fw_nvs_read(&nvs, 0x2002, fw_span_make(&read_calib, sizeof(read_calib)), &read_len);
    assert(st == FW_OK);
    assert(read_calib == calib_val);

    /* 5. Delete Key 0x1001 (Tombstone) */
    st = fw_nvs_delete(&nvs, 0x1001);
    assert(st == FW_OK);

    st = fw_nvs_read(&nvs, 0x1001, fw_span_make(read_buf, sizeof(read_buf)), &read_len);
    assert(st == FW_ERR_NOT_FOUND);

    /* Key 0x2002 must still exist! */
    st = fw_nvs_read(&nvs, 0x2002, fw_span_make(&read_calib, sizeof(read_calib)), &read_len);
    assert(st == FW_OK);
    assert(read_calib == calib_val);

    printf("  -> NVS basic CRUD passed!\n");
}

static void test_nvs_compaction_and_wear_leveling(void) {
    printf("[TEST] NVS: Automatic ping-pong compaction & wear-leveling under stress...\n");

    fw_nvs_t nvs;
    fw_status_t st = fw_nvs_init(&nvs, &s_mock_flash, 0, TEST_SECTOR_SIZE);
    assert(st == FW_OK);
    fw_nvs_format(&nvs);

    /* Store a persistent configuration key that will NOT change */
    const char *device_name = "EDGE_NODE_01";
    assert(fw_nvs_write(&nvs, 0x0001, fw_cspan_make(device_name, strlen(device_name))) == FW_OK);

    /* Write 60 updates to Key 0x0002 (counter) into a 512-byte sector.
     * Each record is ~16 bytes. 60 updates * 16 bytes = 960 bytes,
     * which MUST trigger multiple automatic ping-pong compactions! */
    uint8_t prev_bank = nvs.active_bank;
    uint32_t bank_switches = 0;

    for (uint32_t i = 1; i <= 60; ++i) {
        st = fw_nvs_write(&nvs, 0x0002, fw_cspan_make(&i, sizeof(i)));
        assert(st == FW_OK);

        if (nvs.active_bank != prev_bank) {
            bank_switches++;
            prev_bank = nvs.active_bank;
        }
    }

    assert(bank_switches >= 2); /* Verified multiple ping-pong bank compactions occurred */

    /* Verify persistent Key 0x0001 was migrated across compactions intact */
    char name_buf[32];
    fw_size_t len = 0;
    st = fw_nvs_read(&nvs, 0x0001, fw_span_make(name_buf, sizeof(name_buf)), &len);
    assert(st == FW_OK);
    assert(len == strlen(device_name));
    assert(memcmp(name_buf, device_name, len) == 0);

    /* Verify Key 0x0002 has the latest value (60) */
    uint32_t final_val = 0;
    st = fw_nvs_read(&nvs, 0x0002, fw_span_make(&final_val, sizeof(final_val)), &len);
    assert(st == FW_OK);
    assert(final_val == 60);

    printf("  -> NVS compaction passed! (Bank switches: %u, latest sequence: %u)\n",
        bank_switches, nvs.sequence);
}

static void test_nvs_reboot_and_corruption_recovery(void) {
    printf("[TEST] NVS: Power-loss remount and CRC16 corruption rejection...\n");

    /* 1. Simulate Device Reboot: create fresh fw_nvs_t struct and mount existing flash */
    fw_nvs_t nvs;
    fw_status_t st = fw_nvs_init(&nvs, &s_mock_flash, 0, TEST_SECTOR_SIZE);
    assert(st == FW_OK);

    /* Existing data preserved across reboots */
    char name_buf[32];
    fw_size_t len = 0;
    assert(fw_nvs_read(&nvs, 0x0001, fw_span_make(name_buf, sizeof(name_buf)), &len) == FW_OK);
    assert(memcmp(name_buf, "EDGE_NODE_01", len) == 0);

    /* 2. Write key 0x0003, then deliberately corrupt its payload in flash */
    uint32_t secret = 0xCAFEBABE;
    assert(fw_nvs_write(&nvs, 0x0003, fw_cspan_make(&secret, sizeof(secret))) == FW_OK);

    /* Verify it reads cleanly first */
    uint32_t read_secret = 0;
    assert(fw_nvs_read(&nvs, 0x0003, fw_span_make(&read_secret, sizeof(read_secret)), &len) == FW_OK);
    assert(read_secret == secret);

    /* Find location of secret in virtual flash and corrupt 1 bit */
    for (uint32_t i = 0; i < TEST_SECTOR_SIZE * 2; ++i) {
        if (s_virtual_flash[i] == 0xCA && s_virtual_flash[i+1] == 0xFE) {
            s_virtual_flash[i] = 0x00; /* Corrupt data byte */
            break;
        }
    }

    /* Reading now must return FW_ERR_CORRUPTED due to CRC16 mismatch */
    st = fw_nvs_read(&nvs, 0x0003, fw_span_make(&read_secret, sizeof(read_secret)), &len);
    assert(st == FW_ERR_CORRUPTED);

    printf("  -> NVS reboot and CRC16 corruption recovery passed!\n");
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void) {
    printf("\n====================================================================================\n");
    printf("   ZEROEMBEDDED STORAGE TEST SUITE (Wear-Leveling Dual-Bank Flash NVS)\n");
    printf("====================================================================================\n\n");

    test_nvs_basic_crud();
    test_nvs_compaction_and_wear_leveling();
    test_nvs_reboot_and_corruption_recovery();

    printf("\n\033[1;32m[PASS] ALL STORAGE TESTS PASSED SUCCESSFULLY!\033[0m\n\n");
    return 0;
}
