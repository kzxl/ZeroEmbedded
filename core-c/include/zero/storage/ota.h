#ifndef ZERO_STORAGE_OTA_H
#define ZERO_STORAGE_OTA_H

/**
 * @file ota.h
 * @brief Dual-Bank Bootloader & OTA Firmware Metadata Lifecycle Engine
 * Provides fail-safe OTA state management with 5-stage verification:
 * EMPTY -> DOWNLOADED -> PENDING_VERIFY -> CONFIRMED -> ROLLBACK.
 * Ensures zero-brick immunity across wireless / field firmware upgrades.
 */

#include "zero/types.h"
#include "zero/result.h"
#include "zero/storage/flash.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_OTA_DESC_MAGIC 0x5A45524FU /* 'ZERO' */

#define FW_OTA_STATE_EMPTY          0xFFFFFFFFU /* Staging slot empty or unwritten */
#define FW_OTA_STATE_DOWNLOADED     0x5555AAAAU /* Image downloaded and CRC32 verified */
#define FW_OTA_STATE_PENDING_VERIFY 0xAA55AA55U /* Booted for test, awaiting self-test */
#define FW_OTA_STATE_CONFIRMED      0x12345678U /* Verified and permanently confirmed */
#define FW_OTA_STATE_ROLLBACK       0xDEAD0001U /* Self-test failed -> trigger rollback */

typedef uint32_t fw_ota_state_t;

/**
 * @brief Application descriptor (stored in dedicated Flash metadata sector, 64 bytes).
 */
typedef struct {
    uint32_t magic;         /*!< FW_OTA_DESC_MAGIC */
    uint32_t version_major; /*!< Major version */
    uint32_t version_minor; /*!< Minor version */
    uint32_t version_patch; /*!< Patch version */
    uint32_t image_size;    /*!< Application binary byte size */
    uint32_t image_crc32;   /*!< IEEE 802.3 CRC32 checksum */
    uint32_t state;         /*!< fw_ota_state_t */
    uint32_t boot_attempts; /*!< Number of boots attempted */
    uint32_t max_attempts;  /*!< Maximum allowed boots before forced rollback */
    uint32_t reserved[7];   /*!< 64-byte alignment padding */
} fw_app_desc_t;

/**
 * @brief OTA manager instance structure.
 */
typedef struct {
    const fw_flash_driver_t *flash;
    uint32_t                 metadata_addr;
    fw_app_desc_t            current_desc;
} fw_ota_t;

/**
 * @brief Computes standard IEEE 802.3 CRC32 checksum.
 */
uint32_t fw_crc32_compute(const void *data, fw_size_t length);

/**
 * @brief Initializes the OTA subsystem and reads the current descriptor from Flash.
 */
fw_status_t fw_ota_init(fw_ota_t *ota, const fw_flash_driver_t *flash, uint32_t metadata_addr);

/**
 * @brief Retrieves the active application descriptor.
 */
fw_status_t fw_ota_get_desc(const fw_ota_t *ota, fw_app_desc_t *out_desc);

/**
 * @brief Writes a new application descriptor to the Flash metadata sector.
 */
fw_status_t fw_ota_set_desc(fw_ota_t *ota, const fw_app_desc_t *desc);

/**
 * @brief Reads an application image from Flash and verifies its length and CRC32.
 *
 * @param flash Pointer to flash driver.
 * @param slot_addr Start address of the application slot in Flash.
 * @param desc Expected descriptor containing image_size and image_crc32.
 * @return FW_OK if CRC32 matches, FW_ERR_CORRUPTED otherwise.
 */
fw_status_t fw_ota_verify_image(const fw_flash_driver_t *flash,
                               uint32_t slot_addr,
                               const fw_app_desc_t *desc);

/**
 * @brief Transitions state to PENDING_VERIFY before jumping into newly updated firmware.
 */
fw_status_t fw_ota_mark_pending_verify(fw_ota_t *ota);

/**
 * @brief Application self-test confirms the image is healthy; transitions to CONFIRMED.
 */
fw_status_t fw_ota_confirm(fw_ota_t *ota);

/**
 * @brief Reverts to previous stable bank if health checks fail; transitions to ROLLBACK.
 */
fw_status_t fw_ota_rollback(fw_ota_t *ota);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_STORAGE_OTA_H */
