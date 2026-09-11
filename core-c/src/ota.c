/**
 * @file ota.c
 * @brief Dual-Bank Bootloader & OTA Metadata Lifecycle implementation
 */

#include "zero/storage/ota.h"
#include <string.h>

static uint32_t crc32_step(uint32_t crc, const uint8_t *p, fw_size_t len) {
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ (0xEDB88320U & (-(int32_t)(crc & 1)));
        }
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
