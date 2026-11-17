/**
 * @file ota.c
 * @brief Dual-Bank Bootloader & OTA Metadata Lifecycle implementation
 */

#include "zero/storage/ota.h"
#include <string.h>

/* IEEE 802.3 CRC32 4-bit nibble LUT (64 bytes Flash, 4x faster than bit-by-bit) */
static const uint32_t s_crc32_nibble_lut[16] = {
    0x00000000U, 0x1DB71064U, 0x3B6E20C8U, 0x26D930ACU,
    0x76DC4190U, 0x6B6B51F4U, 0x4DB26158U, 0x5005713CU,
    0xEDB88320U, 0xF00F9344U, 0xD6D6A3E8U, 0xCB61B38CU,
    0x9B64C2B0U, 0x86D3D2D4U, 0xA00AE278U, 0xBDBDF21CU
};

static uint32_t crc32_step(uint32_t crc, const uint8_t *p, fw_size_t len) {
    while (len--) {
        uint8_t b = *p++;
        crc = (crc >> 4) ^ s_crc32_nibble_lut[(crc ^ b) & 0x0FU];
        crc = (crc >> 4) ^ s_crc32_nibble_lut[(crc ^ (b >> 4)) & 0x0FU];
    }
    return crc;
}

uint32_t fw_crc32_compute(const void *data, fw_size_t length) {
    if (data == FW_NULL || length == 0) {
        return 0;
    }
    return ~crc32_step(0xFFFFFFFFU, (const uint8_t*)data, length);
}

fw_status_t fw_ota_init(fw_ota_t *ota, const fw_flash_driver_t *flash, uint32_t metadata_addr) {
    if (ota == FW_NULL || flash == FW_NULL || flash->read == FW_NULL ||
        flash->write == FW_NULL || flash->erase_sector == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    ota->flash         = flash;
    ota->metadata_addr = metadata_addr;

    /* Read existing descriptor from Flash */
    fw_app_desc_t desc;
    FW_CHECK(flash->read(flash->context, metadata_addr, &desc, sizeof(desc)));

    if (desc.magic != FW_OTA_DESC_MAGIC) {
        /* Uninitialized metadata sector -> initialize defaults */
        memset(&desc, 0, sizeof(desc));
        desc.magic         = FW_OTA_DESC_MAGIC;
        desc.version_major = 0;
        desc.version_minor = 1;
        desc.version_patch = 0;
        desc.state         = FW_OTA_STATE_EMPTY;
        desc.max_attempts  = 3;

        FW_CHECK(fw_ota_set_desc(ota, &desc));
    }

    ota->current_desc = desc;
    return FW_OK;
}

fw_status_t fw_ota_get_desc(const fw_ota_t *ota, fw_app_desc_t *out_desc) {
    if (ota == FW_NULL || out_desc == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }
    *out_desc = ota->current_desc;
    return FW_OK;
}

fw_status_t fw_ota_set_desc(fw_ota_t *ota, const fw_app_desc_t *desc) {
    if (ota == FW_NULL || desc == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    /* Erase metadata sector before writing */
    FW_CHECK(ota->flash->erase_sector(ota->flash->context, ota->metadata_addr));

    /* Write new descriptor */
    FW_CHECK(ota->flash->write(ota->flash->context, ota->metadata_addr, desc, sizeof(fw_app_desc_t)));

    ota->current_desc = *desc;
    return FW_OK;
}

fw_status_t fw_ota_verify_image(const fw_flash_driver_t *flash,
                               uint32_t slot_addr,
                               const fw_app_desc_t *desc) {
    if (flash == FW_NULL || flash->read == FW_NULL || desc == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    if (desc->image_size == 0) {
        return FW_ERR_INVALID_ARG;
    }

    uint8_t chunk_buf[128];
    uint32_t remaining = desc->image_size;
    uint32_t curr_addr = slot_addr;
    uint32_t crc_acc = 0xFFFFFFFFU;

    while (remaining > 0) {
        fw_size_t to_read = (remaining > sizeof(chunk_buf)) ? sizeof(chunk_buf) : remaining;
        FW_CHECK(flash->read(flash->context, curr_addr, chunk_buf, to_read));

        crc_acc = crc32_step(crc_acc, chunk_buf, to_read);
        curr_addr += (uint32_t)to_read;
        remaining -= (uint32_t)to_read;
    }

    uint32_t final_crc = ~crc_acc;
    if (final_crc != desc->image_crc32) {
        return FW_ERR_CORRUPTED;
    }

    return FW_OK;
}

fw_status_t fw_ota_mark_pending_verify(fw_ota_t *ota) {
    if (ota == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    fw_app_desc_t desc = ota->current_desc;
    if (desc.max_attempts > 0U && desc.boot_attempts >= desc.max_attempts) {
        desc.state = FW_OTA_STATE_ROLLBACK;
        FW_CHECK(fw_ota_set_desc(ota, &desc));
        return FW_ERR_TIMEOUT;
    }

    desc.state = FW_OTA_STATE_PENDING_VERIFY;
    desc.boot_attempts++;

    return fw_ota_set_desc(ota, &desc);
}

fw_status_t fw_ota_confirm(fw_ota_t *ota) {
    if (ota == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    fw_app_desc_t desc = ota->current_desc;
    desc.state = FW_OTA_STATE_CONFIRMED;
    desc.boot_attempts = 0;

    return fw_ota_set_desc(ota, &desc);
}

fw_status_t fw_ota_rollback(fw_ota_t *ota) {
    if (ota == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    fw_app_desc_t desc = ota->current_desc;
    desc.state = FW_OTA_STATE_ROLLBACK;

    return fw_ota_set_desc(ota, &desc);
}
