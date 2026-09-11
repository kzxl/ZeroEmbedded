#include "zero/protocol/zerowire.h"
#include <string.h>

uint16_t fw_crc16_ccitt(const void *data, fw_size_t length) {
    if (data == FW_NULL || length == 0) return 0;
    const uint8_t *bytes = (const uint8_t*)data;
    uint16_t crc = 0xFFFF;

    for (fw_size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)(bytes[i] << 8);
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

fw_size_t fw_zerowire_encode(const fw_zerowire_frame_t *frame, fw_span_t out_buf) {
    if (frame == FW_NULL || out_buf.data == FW_NULL) return 0;
    if (frame->length > ZEROWIRE_MAX_PAYLOAD) return 0;

    fw_size_t total_size = ZEROWIRE_HEADER_SIZE + frame->length + ZEROWIRE_CRC_SIZE;
    if (out_buf.length < total_size) return 0;

    uint8_t *dst = (uint8_t*)out_buf.data;
    dst[0] = ZEROWIRE_SOF0;
    dst[1] = ZEROWIRE_SOF1;
    dst[2] = frame->seq;
    dst[3] = frame->msg_id;
    dst[4] = (uint8_t)(frame->length & 0xFF);
    dst[5] = (uint8_t)((frame->length >> 8) & 0xFF);

    if (frame->length > 0) {
        memcpy(&dst[6], frame->payload, frame->length);
    }

    uint16_t crc = fw_crc16_ccitt(dst, ZEROWIRE_HEADER_SIZE + frame->length);
    dst[total_size - 2] = (uint8_t)(crc & 0xFF);
    dst[total_size - 1] = (uint8_t)((crc >> 8) & 0xFF);

    return total_size;
}

fw_status_t fw_zerowire_decode(fw_cspan_t raw_data, fw_zerowire_frame_t *out_frame) {
    if (raw_data.data == FW_NULL || out_frame == FW_NULL) return FW_ERR_INVALID_ARG;
    if (raw_data.length < ZEROWIRE_HEADER_SIZE + ZEROWIRE_CRC_SIZE) return FW_ERR_NOT_FOUND;

    const uint8_t *src = (const uint8_t*)raw_data.data;
    if (src[0] != ZEROWIRE_SOF0 || src[1] != ZEROWIRE_SOF1) {
        return FW_ERR_CORRUPTED;
    }

    uint16_t payload_len = (uint16_t)(src[4] | (src[5] << 8));
    if (payload_len > ZEROWIRE_MAX_PAYLOAD) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    fw_size_t total_expected = ZEROWIRE_HEADER_SIZE + payload_len + ZEROWIRE_CRC_SIZE;
    if (raw_data.length < total_expected) {
        return FW_ERR_NOT_FOUND; /* Incomplete packet */
    }

    /* Verify CRC */
    uint16_t expected_crc = fw_crc16_ccitt(src, ZEROWIRE_HEADER_SIZE + payload_len);
    uint16_t actual_crc = (uint16_t)(src[total_expected - 2] | (src[total_expected - 1] << 8));
    if (expected_crc != actual_crc) {
        return FW_ERR_CORRUPTED;
    }

    out_frame->seq = src[2];
    out_frame->msg_id = src[3];
    out_frame->length = payload_len;
    if (payload_len > 0) {
        memcpy(out_frame->payload, &src[6], payload_len);
    }

    return FW_OK;
}

fw_status_t fw_zerowire_stream_sync(
    fw_cspan_t stream_data,
    fw_zerowire_frame_t *out_frame,
    fw_size_t *out_consumed
) {
    if (stream_data.data == FW_NULL || out_frame == FW_NULL || out_consumed == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    *out_consumed = 0;
    const uint8_t *bytes = (const uint8_t*)stream_data.data;
    fw_size_t len = stream_data.length;

    /* Scan forward for SOF pattern: 0xAA, 0x55 */
    for (fw_size_t i = 0; i + 1 < len; ++i) {
        if (bytes[i] == ZEROWIRE_SOF0 && bytes[i + 1] == ZEROWIRE_SOF1) {
            /* Attempt decode from this offset */
            fw_cspan_t candidate = fw_cspan_make((const void*)(bytes + i), len - i);
            fw_status_t st = fw_zerowire_decode(candidate, out_frame);
            if (st == FW_OK) {
                *out_consumed = i + ZEROWIRE_HEADER_SIZE + out_frame->length + ZEROWIRE_CRC_SIZE;
                return FW_OK;
            } else if (st == FW_ERR_NOT_FOUND) {
                /* Incomplete frame detected, skip noise up to SOF */
                *out_consumed = i;
                return FW_ERR_NOT_FOUND;
            }
            /* If corrupted, continue scanning for next SOF */
        }
    }

    /* No SOF found in stream buffer; consume all except last byte (which might be 0xAA) */
    *out_consumed = len > 0 ? (len - 1) : 0;
    return FW_ERR_NOT_FOUND;
}
