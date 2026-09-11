#ifndef ZERO_STORAGE_FLASH_H
#define ZERO_STORAGE_FLASH_H

/**
 * @file flash.h
 * @brief Flash memory hardware driver abstraction layer.
 * Decouples persistent storage modules (NVS, Bootloader) from specific hardware
 * (MCU internal Flash, external SPI NOR Flash, or memory-mapped simulation).
 */

#include "zero/types.h"
#include "zero/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef fw_status_t (*fw_flash_read_fn)(void *ctx, uint32_t addr, void *buf, fw_size_t len);
typedef fw_status_t (*fw_flash_write_fn)(void *ctx, uint32_t addr, const void *buf, fw_size_t len);
typedef fw_status_t (*fw_flash_erase_fn)(void *ctx, uint32_t sector_addr);

typedef struct {
    uint32_t            sector_size;   /*!< Sector erase size in bytes (e.g. 1024, 2048, 4096) */
    fw_flash_read_fn    read;          /*!< Read callback */
    fw_flash_write_fn   write;         /*!< Write callback (typically 4-byte or 8-byte aligned) */
    fw_flash_erase_fn   erase_sector;  /*!< Erase callback (sets sector memory to 0xFF) */
    void               *context;       /*!< Optional hardware instance context */
} fw_flash_driver_t;

#ifdef __cplusplus
}
#endif

#endif /* ZERO_STORAGE_FLASH_H */
