#ifndef ZERO_PROTOCOL_ZEROWIRE_H
#define ZERO_PROTOCOL_ZEROWIRE_H

/**
 * @file zerowire.h
 * @brief ZeroWire deterministic binary framing protocol connecting MCU to ZeroPlatform.
 *
 * Frame Format:
 * [0..1] SOF (0xAA, 0x55)
 * [2]    Sequence (uint8)
 * [3]    MsgID (uint8)
 * [4..5] Payload Length (uint16_t Little-Endian)
 * [6..N] Payload Data (0..256 bytes)
 * [N+1..N+2] CRC16-CCITT (uint16_t Little-Endian)
 */

#include "../types.h"
#include "../result.h"
#include "../span.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZEROWIRE_SOF0 0xAA
#define ZEROWIRE_SOF1 0x55
#define ZEROWIRE_HEADER_SIZE 6
#define ZEROWIRE_CRC_SIZE    2
#define ZEROWIRE_MAX_PAYLOAD 256

typedef struct {
    uint8_t   seq;
    uint8_t   msg_id;
    uint16_t  length;
    uint8_t   payload[ZEROWIRE_MAX_PAYLOAD];
} fw_zerowire_frame_t;

/** Calculates CRC16-CCITT (poly 0x1021, init 0xFFFF) */
uint16_t fw_crc16_ccitt(const void *data, fw_size_t length);

/**
 * @brief Encodes a frame into a target buffer span.
 * @return Number of encoded bytes written, or 0 on buffer too small.
 */
fw_size_t fw_zerowire_encode(const fw_zerowire_frame_t *frame, fw_span_t out_buf);

/**
 * @brief Decodes a raw span into a frame.
 * @return FW_OK on success, FW_ERR_CORRUPTED on CRC mismatch, FW_ERR_NOT_FOUND if incomplete.
 */
fw_status_t fw_zerowire_decode(fw_cspan_t raw_data, fw_zerowire_frame_t *out_frame);

/**
 * @brief Scans a stream buffer for the next valid ZeroWire frame, skipping noise bytes.
 * @param stream_data Contiguous stream slice.
 * @param out_frame Decoded frame destination.
 * @param out_consumed Number of bytes consumed from stream_data.
 * @return FW_OK on success, FW_ERR_NOT_FOUND if incomplete, FW_ERR_CORRUPTED if invalid.
 */
fw_status_t fw_zerowire_stream_sync(
    fw_cspan_t stream_data,
    fw_zerowire_frame_t *out_frame,
    fw_size_t *out_consumed
);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_PROTOCOL_ZEROWIRE_H */
