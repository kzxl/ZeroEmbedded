/**
 * @file nvs.c
 * @brief Wear-Leveling Dual-Bank Ping-Pong Flash Key-Value Storage implementation
 */

#include "zero/storage/nvs.h"
#include <string.h>

#define ALIGN_UP_4(x) (((uint32_t)(x) + 3U) & ~3U)

/* Fast, self-contained CRC16-CCITT (poly 0x1021, init 0xFFFF) */
static uint16_t crc16_step(uint16_t crc, const uint8_t *data, fw_size_t len) {
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static uint16_t calc_record_crc(uint16_t key, uint16_t length, const void *payload) {
    uint8_t meta[4];
    meta[0] = (uint8_t)(key & 0xFF);
    meta[1] = (uint8_t)((key >> 8) & 0xFF);
    meta[2] = (uint8_t)(length & 0xFF);
    meta[3] = (uint8_t)((length >> 8) & 0xFF);

    uint16_t crc = crc16_step(0xFFFF, meta, 4);
    if (length > 0 && payload != FW_NULL) {
        crc = crc16_step(crc, (const uint8_t*)payload, length);
    }
    return crc;
}

static fw_status_t flash_read(const fw_nvs_t *nvs, uint32_t addr, void *buf, fw_size_t len) {
    return nvs->flash->read(nvs->flash->context, addr, buf, len);
}

static fw_status_t flash_write(const fw_nvs_t *nvs, uint32_t addr, const void *buf, fw_size_t len) {
    return nvs->flash->write(nvs->flash->context, addr, buf, len);
}

static fw_status_t flash_erase(const fw_nvs_t *nvs, uint32_t sector_addr) {
    return nvs->flash->erase_sector(nvs->flash->context, sector_addr);
}

/* Scans active bank to find next available write offset */
static void scan_write_offset(fw_nvs_t *nvs) {
    uint32_t base = nvs->bank_addr[nvs->active_bank];
    uint32_t offset = sizeof(fw_nvs_sector_hdr_t);
    uint32_t max_size = nvs->flash->sector_size;

    while (offset + sizeof(fw_nvs_record_hdr_t) <= max_size) {
        fw_nvs_record_hdr_t hdr;
        if (flash_read(nvs, base + offset, &hdr, sizeof(hdr)) != FW_OK) {
            break;
        }

        /* Unwritten empty Flash word signifies end of written log */
        if (hdr.key == FW_NVS_KEY_EMPTY) {
            nvs->write_offset = offset;
            return;
        }

        uint32_t rec_total = sizeof(fw_nvs_record_hdr_t) + ALIGN_UP_4(hdr.length);
        if (offset + rec_total > max_size) {
            /* Corrupted header or sector boundary reached */
            break;
        }

        offset += rec_total;
    }

    nvs->write_offset = offset;
}

fw_status_t fw_nvs_format(fw_nvs_t *nvs) {
    if (nvs == FW_NULL || nvs->flash == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    /* 1. Erase Bank 0 and Bank 1 */
    FW_CHECK(flash_erase(nvs, nvs->bank_addr[0]));
    FW_CHECK(flash_erase(nvs, nvs->bank_addr[1]));

    /* 2. Write Bank 0 header as ACTIVE */
    fw_nvs_sector_hdr_t hdr;
    hdr.magic    = FW_NVS_MAGIC;
    hdr.state    = FW_NVS_SECTOR_ACTIVE;
    hdr.sequence = 1;
    hdr.reserved = 0xFFFFFFFFU;

    FW_CHECK(flash_write(nvs, nvs->bank_addr[0], &hdr, sizeof(hdr)));

    nvs->active_bank  = 0;
    nvs->write_offset = sizeof(fw_nvs_sector_hdr_t);
    nvs->sequence     = 1;

    return FW_OK;
}

fw_status_t fw_nvs_init(fw_nvs_t *nvs,
                        const fw_flash_driver_t *flash,
                        uint32_t bank0_addr,
                        uint32_t bank1_addr) {
    if (nvs == FW_NULL || flash == FW_NULL || flash->read == FW_NULL ||
        flash->write == FW_NULL || flash->erase_sector == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    if (flash->sector_size < 256) {
        return FW_ERR_INVALID_ARG;
    }

    nvs->flash        = flash;
    nvs->bank_addr[0] = bank0_addr;
    nvs->bank_addr[1] = bank1_addr;

    /* Read headers of both banks to determine active bank and recover */
    fw_nvs_sector_hdr_t hdr0, hdr1;
    FW_CHECK(flash_read(nvs, bank0_addr, &hdr0, sizeof(hdr0)));
    FW_CHECK(flash_read(nvs, bank1_addr, &hdr1, sizeof(hdr1)));

    fw_bool_t valid0 = (hdr0.magic == FW_NVS_MAGIC && hdr0.state == FW_NVS_SECTOR_ACTIVE);
    fw_bool_t valid1 = (hdr1.magic == FW_NVS_MAGIC && hdr1.state == FW_NVS_SECTOR_ACTIVE);

    if (!valid0 && !valid1) {
        /* Both banks uninitialized -> automatically format */
        return fw_nvs_format(nvs);
    } else if (valid0 && !valid1) {
        nvs->active_bank = 0;
        nvs->sequence    = hdr0.sequence;
    } else if (!valid0 && valid1) {
        nvs->active_bank = 1;
        nvs->sequence    = hdr1.sequence;
    } else {
        /* Both active (interrupted compaction) -> pick bank with newest sequence */
        if (hdr1.sequence > hdr0.sequence) {
            nvs->active_bank = 1;
            nvs->sequence    = hdr1.sequence;
            flash_erase(nvs, bank0_addr);
        } else {
            nvs->active_bank = 0;
            nvs->sequence    = hdr0.sequence;
            flash_erase(nvs, bank1_addr);
        }
    }

    scan_write_offset(nvs);
    return FW_OK;
}

/* Performs Garbage Collection compaction: copies latest unique keys from active bank to alt bank */
static fw_status_t nvs_compact(fw_nvs_t *nvs) {
    uint8_t alt_bank = (uint8_t)(1 - nvs->active_bank);
    uint32_t src_base = nvs->bank_addr[nvs->active_bank];
    uint32_t dst_base = nvs->bank_addr[alt_bank];
    uint32_t max_size = nvs->flash->sector_size;

    /* 1. Erase destination bank */
    FW_CHECK(flash_erase(nvs, dst_base));

    /* 2. Write destination header with incremented sequence */
    uint32_t next_seq = nvs->sequence + 1;
    fw_nvs_sector_hdr_t dst_hdr;
    dst_hdr.magic    = FW_NVS_MAGIC;
    dst_hdr.state    = FW_NVS_SECTOR_ACTIVE;
    dst_hdr.sequence = next_seq;
    dst_hdr.reserved = 0xFFFFFFFFU;
    FW_CHECK(flash_write(nvs, dst_base, &dst_hdr, sizeof(dst_hdr)));

    uint32_t dst_offset = sizeof(fw_nvs_sector_hdr_t);

    /* 3. Scan active bank and migrate only latest valid unique records */
    uint32_t src_offset = sizeof(fw_nvs_sector_hdr_t);
    while (src_offset < nvs->write_offset) {
        fw_nvs_record_hdr_t cur_hdr;
        if (flash_read(nvs, src_base + src_offset, &cur_hdr, sizeof(cur_hdr)) != FW_OK) {
            break;
        }

        if (cur_hdr.key == FW_NVS_KEY_EMPTY) {
            break;
        }

        uint32_t cur_total = sizeof(fw_nvs_record_hdr_t) + ALIGN_UP_4(cur_hdr.length);

        if (cur_hdr.status == FW_NVS_STATUS_VALID) {
            /* Check if this key was superseded by a later record in the log */
            fw_bool_t is_latest = FW_TRUE;
            uint32_t scan_off = src_offset + cur_total;

            while (scan_off < nvs->write_offset) {
                fw_nvs_record_hdr_t next_hdr;
                if (flash_read(nvs, src_base + scan_off, &next_hdr, sizeof(next_hdr)) != FW_OK) {
                    break;
                }
                if (next_hdr.key == FW_NVS_KEY_EMPTY) {
                    break;
                }
                if (next_hdr.key == cur_hdr.key) {
                    is_latest = FW_FALSE;
                    break;
                }
                scan_off += sizeof(fw_nvs_record_hdr_t) + ALIGN_UP_4(next_hdr.length);
            }

            if (is_latest) {
                /* Copy this live record to destination bank */
                if (dst_offset + cur_total > max_size) {
                    return FW_ERR_OUT_OF_MEMORY;
                }

                uint8_t copy_buf[128];
                uint32_t bytes_to_copy = cur_total;
                uint32_t chunk_src = src_base + src_offset;
                uint32_t chunk_dst = dst_base + dst_offset;

                while (bytes_to_copy > 0) {
                    uint32_t chunk = (bytes_to_copy > sizeof(copy_buf)) ? sizeof(copy_buf) : bytes_to_copy;
                    FW_CHECK(flash_read(nvs, chunk_src, copy_buf, chunk));
                    FW_CHECK(flash_write(nvs, chunk_dst, copy_buf, chunk));
                    chunk_src += chunk;
                    chunk_dst += chunk;
                    bytes_to_copy -= chunk;
                }

                dst_offset += cur_total;
            }
        }

        src_offset += cur_total;
    }

    /* 4. Erase old source bank */
    FW_CHECK(flash_erase(nvs, src_base));

    /* 5. Switch active bank to newly compacted bank */
    nvs->active_bank  = alt_bank;
    nvs->write_offset = dst_offset;
    nvs->sequence     = next_seq;

    return FW_OK;
}

fw_status_t fw_nvs_write(fw_nvs_t *nvs, uint16_t key, fw_cspan_t data) {
    if (nvs == FW_NULL || key == FW_NVS_KEY_EMPTY) {
        return FW_ERR_INVALID_ARG;
    }

    uint32_t payload_len = (uint32_t)data.length;
    uint32_t aligned_payload = ALIGN_UP_4(payload_len);
    uint32_t rec_total = sizeof(fw_nvs_record_hdr_t) + aligned_payload;

    if (sizeof(fw_nvs_sector_hdr_t) + rec_total > nvs->flash->sector_size) {
        return FW_ERR_OUT_OF_MEMORY;
    }

    /* Check if compaction is required */
    if (nvs->write_offset + rec_total > nvs->flash->sector_size) {
        FW_CHECK(nvs_compact(nvs));

        if (nvs->write_offset + rec_total > nvs->flash->sector_size) {
            return FW_ERR_OUT_OF_MEMORY;
        }
    }

    /* Prepare record header */
    fw_nvs_record_hdr_t hdr;
    hdr.key    = key;
    hdr.length = (uint16_t)payload_len;
    hdr.crc16  = calc_record_crc(key, (uint16_t)payload_len, data.data);
    hdr.status = FW_NVS_STATUS_VALID;

    uint32_t base = nvs->bank_addr[nvs->active_bank];
    uint32_t addr = base + nvs->write_offset;

    FW_CHECK(flash_write(nvs, addr, &hdr, sizeof(hdr)));

    if (payload_len > 0) {
        /* Zero padding bytes for 4-byte flash programming alignment */
        uint8_t pad_buf[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
        FW_CHECK(flash_write(nvs, addr + sizeof(hdr), data.data, payload_len));

        if (aligned_payload > payload_len) {
            uint32_t pad_len = aligned_payload - payload_len;
            FW_CHECK(flash_write(nvs, addr + sizeof(hdr) + payload_len, pad_buf, pad_len));
        }
    }

    nvs->write_offset += rec_total;
    return FW_OK;
}

fw_status_t fw_nvs_read(fw_nvs_t *nvs, uint16_t key, fw_span_t out_data, fw_size_t *out_len) {
    if (nvs == FW_NULL || key == FW_NVS_KEY_EMPTY) {
        return FW_ERR_INVALID_ARG;
    }

    uint32_t base = nvs->bank_addr[nvs->active_bank];
    uint32_t offset = sizeof(fw_nvs_sector_hdr_t);

    uint32_t match_offset = 0;
    fw_nvs_record_hdr_t match_hdr;
    memset(&match_hdr, 0, sizeof(match_hdr));
    fw_bool_t found = FW_FALSE;

    /* Scan forward to find latest version of key */
    while (offset < nvs->write_offset) {
        fw_nvs_record_hdr_t hdr;
        if (flash_read(nvs, base + offset, &hdr, sizeof(hdr)) != FW_OK) {
            break;
        }

        if (hdr.key == FW_NVS_KEY_EMPTY) {
            break;
        }

        uint32_t rec_total = sizeof(fw_nvs_record_hdr_t) + ALIGN_UP_4(hdr.length);

        if (hdr.key == key) {
            if (hdr.status == FW_NVS_STATUS_VALID) {
                found        = FW_TRUE;
                match_offset = offset;
                match_hdr    = hdr;
            } else {
                /* Key was explicitly deleted */
                found = FW_FALSE;
            }
        }

        offset += rec_total;
    }

    if (!found) {
        return FW_ERR_NOT_FOUND;
    }

    if (out_data.length < match_hdr.length) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    if (match_hdr.length > 0) {
        FW_CHECK(flash_read(nvs, base + match_offset + sizeof(fw_nvs_record_hdr_t),
                            out_data.data, match_hdr.length));
    }

    /* Verify CRC16 */
    uint16_t check_crc = calc_record_crc(key, match_hdr.length, out_data.data);
    if (check_crc != match_hdr.crc16) {
        return FW_ERR_CORRUPTED;
    }

    if (out_len != FW_NULL) {
        *out_len = match_hdr.length;
    }

    return FW_OK;
}

fw_status_t fw_nvs_delete(fw_nvs_t *nvs, uint16_t key) {
    if (nvs == FW_NULL || key == FW_NVS_KEY_EMPTY) {
        return FW_ERR_INVALID_ARG;
    }

    /* Check if key currently exists */
    uint8_t dummy[1];
    fw_span_t dummy_span = FW_SPAN_FROM_ARRAY(dummy);
    fw_size_t len = 0;
    if (fw_nvs_read(nvs, key, dummy_span, &len) == FW_ERR_NOT_FOUND) {
        return FW_ERR_NOT_FOUND;
    }

    /* Append a tombstone record with FW_NVS_STATUS_DEAD */
    uint32_t rec_total = sizeof(fw_nvs_record_hdr_t);

    if (nvs->write_offset + rec_total > nvs->flash->sector_size) {
        FW_CHECK(nvs_compact(nvs));
    }

    fw_nvs_record_hdr_t hdr;
    hdr.key    = key;
    hdr.length = 0;
    hdr.crc16  = calc_record_crc(key, 0, FW_NULL);
    hdr.status = FW_NVS_STATUS_DEAD;

    uint32_t base = nvs->bank_addr[nvs->active_bank];
    FW_CHECK(flash_write(nvs, base + nvs->write_offset, &hdr, sizeof(hdr)));

    nvs->write_offset += rec_total;
    return FW_OK;
}

fw_status_t fw_nvs_get_stats(const fw_nvs_t *nvs, fw_size_t *out_used_bytes, fw_size_t *out_free_bytes) {
    if (nvs == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    if (out_used_bytes != FW_NULL) {
        *out_used_bytes = nvs->write_offset;
    }
    if (out_free_bytes != FW_NULL) {
        *out_free_bytes = (nvs->flash->sector_size > nvs->write_offset) ?
                          (nvs->flash->sector_size - nvs->write_offset) : 0;
    }

    return FW_OK;
}
