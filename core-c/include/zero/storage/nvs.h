#ifndef ZERO_STORAGE_NVS_H
#define ZERO_STORAGE_NVS_H

/**
 * @file nvs.h
 * @brief Wear-Leveling Dual-Bank Ping-Pong Flash Key-Value Storage (NVS)
 * Zero-heap, power-fail resilient parameter storage with CRC16 integrity,
 * sequential append writes, and automatic background compaction.
 */

#include "zero/types.h"
#include "zero/result.h"
#include "zero/span.h"
#include "zero/storage/flash.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_NVS_MAGIC         0x5A45524FU /* 'ZERO' */
#define FW_NVS_KEY_EMPTY     0xFFFFU
#define FW_NVS_STATUS_VALID  0xFFFFU
#define FW_NVS_STATUS_DEAD   0x0000U

typedef enum {
    FW_NVS_SECTOR_ERASED = 0xFFFFFFFFU,
    FW_NVS_SECTOR_ACTIVE = 0x5555AAAAU,
    FW_NVS_SECTOR_FULL   = 0x00000000U
} fw_nvs_sector_state_t;

/**
 * @brief 16-byte sector header located at the start of each bank.
 */
typedef struct {
    uint32_t magic;      /*!< Magic number: FW_NVS_MAGIC */
    uint32_t state;      /*!< Sector state: ERASED, ACTIVE, or FULL */
    uint32_t sequence;   /*!< Monotonic epoch sequence counter */
    uint32_t reserved;   /*!< Alignment padding */
} fw_nvs_sector_hdr_t;

/**
 * @brief 8-byte record header preceding each stored key-value pair.
 */
typedef struct {
    uint16_t key;        /*!< Unique identifier: 1..0xFFFE */
    uint16_t length;     /*!< Payload byte count */
    uint16_t crc16;      /*!< CRC16-CCITT of (key + length + payload) */
    uint16_t status;     /*!< FW_NVS_STATUS_VALID or FW_NVS_STATUS_DEAD */
} fw_nvs_record_hdr_t;

/**
 * @brief NVS Storage instance structure (~32 bytes in RAM/BSS).
 */
typedef struct {
    const fw_flash_driver_t *flash;
    uint32_t                 bank_addr[2];
    uint8_t                  active_bank;   /*!< 0 or 1 */
    uint32_t                 write_offset;  /*!< Next write position inside active bank */
    uint32_t                 sequence;      /*!< Active bank sequence number */
} fw_nvs_t;

/**
 * @brief Initializes NVS storage and mounts/recovers active bank state.
 *
 * @param nvs Pointer to NVS instance.
 * @param flash Pointer to flash driver.
 * @param bank0_addr Start address of Bank 0 (must be aligned to flash->sector_size).
 * @param bank1_addr Start address of Bank 1 (must be aligned to flash->sector_size).
 * @return FW_OK on success; FW_ERR_INVALID_ARG on invalid parameters; FW_ERR_CORRUPTED if formatting needed.
 */
fw_status_t fw_nvs_init(fw_nvs_t *nvs,
                        const fw_flash_driver_t *flash,
                        uint32_t bank0_addr,
                        uint32_t bank1_addr);

/**
 * @brief Formats NVS by erasing both banks and activating Bank 0.
 */
fw_status_t fw_nvs_format(fw_nvs_t *nvs);

/**
 * @brief Writes or updates a key-value record.
 * Appends sequentially. If active bank is full, triggers compaction to alternate bank.
 *
 * @param nvs Pointer to NVS instance.
 * @param key Record key (1..0xFFFE).
 * @param data Read-only span containing binary payload.
 * @return FW_OK on success, FW_ERR_OUT_OF_MEMORY if record exceeds sector capacity.
 */
fw_status_t fw_nvs_write(fw_nvs_t *nvs, uint16_t key, fw_cspan_t data);

/**
 * @brief Reads the latest valid version of a key-value record.
 *
 * @param nvs Pointer to NVS instance.
 * @param key Record key to find.
 * @param out_data Destination buffer span.
 * @param out_len Pointer to receive actual payload length.
 * @return FW_OK if found and CRC16 verified; FW_ERR_NOT_FOUND if key does not exist.
 */
fw_status_t fw_nvs_read(fw_nvs_t *nvs, uint16_t key, fw_span_t out_data, fw_size_t *out_len);

/**
 * @brief Deletes a key by appending a tombstone record (status = FW_NVS_STATUS_DEAD).
 */
fw_status_t fw_nvs_delete(fw_nvs_t *nvs, uint16_t key);

/**
 * @brief Queries storage capacity and used byte count in active bank.
 */
fw_status_t fw_nvs_get_stats(const fw_nvs_t *nvs, fw_size_t *out_used_bytes, fw_size_t *out_free_bytes);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_STORAGE_NVS_H */
