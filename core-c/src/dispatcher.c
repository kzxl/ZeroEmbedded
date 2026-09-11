#include "zero/protocol/dispatcher.h"
#include <string.h>

fw_status_t fw_cmd_table_init(
    fw_cmd_table_t *table,
    const fw_cmd_entry_t *entries,
    fw_size_t entry_count
) {
    if (table == FW_NULL || entries == FW_NULL || entry_count == 0) {
        return FW_ERR_INVALID_ARG;
    }

    table->entries = entries;
    table->entry_count = entry_count;

    return FW_OK;
}

fw_status_t fw_cmd_dispatch(
    const fw_cmd_table_t *table,
    const fw_zerowire_frame_t *req,
    fw_zerowire_frame_t *out_resp,
    fw_bool_t *out_has_response
) {
    if (table == FW_NULL || req == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    if (out_has_response != FW_NULL) {
        *out_has_response = FW_FALSE;
    }

    /* Search command table for matching msg_id */
    for (fw_size_t i = 0; i < table->entry_count; ++i) {
        if (table->entries[i].msg_id == req->msg_id) {
            const fw_cmd_entry_t *entry = &table->entries[i];

            /* Validate minimum payload length */
            if (req->length < entry->min_payload_len) {
                return FW_ERR_INVALID_ARG;
            }

            if (entry->handler == FW_NULL) {
                return FW_OK; /* No handler bound */
            }

            fw_cspan_t req_span = fw_cspan_make(req->payload, req->length);
            uint8_t resp_payload[ZEROWIRE_MAX_PAYLOAD];
            fw_span_t resp_span = FW_SPAN_FROM_ARRAY(resp_payload);
            fw_size_t resp_len = 0;

            fw_status_t st = entry->handler(req->seq, req_span, resp_span, &resp_len);
            if (st != FW_OK) {
                return st;
            }

            /* Build response frame if payload produced and destination provided */
            if (resp_len > 0 && out_resp != FW_NULL) {
                out_resp->seq = req->seq;
                out_resp->msg_id = req->msg_id | 0x80; /* Response flag high-bit convention */
                out_resp->length = (uint16_t)resp_len;
                memcpy(out_resp->payload, resp_payload, resp_len);

                if (out_has_response != FW_NULL) {
                    *out_has_response = FW_TRUE;
                }
            }

            return FW_OK;
        }
    }

    return FW_ERR_NOT_FOUND; /* Unknown MsgID */
}

fw_status_t fw_cmd_process_stream(
    const fw_cmd_table_t *table,
    fw_cspan_t rx_stream,
    fw_size_t *out_consumed,
    fw_span_t tx_out_buf,
    fw_size_t *out_tx_len
) {
    if (out_tx_len != FW_NULL) {
        *out_tx_len = 0;
    }

    fw_zerowire_frame_t req_frame;
    fw_status_t sync_st = fw_zerowire_stream_sync(rx_stream, &req_frame, out_consumed);
    if (sync_st != FW_OK) {
        return sync_st;
    }

    fw_zerowire_frame_t resp_frame;
    fw_bool_t has_resp = FW_FALSE;
    fw_status_t dispatch_st = fw_cmd_dispatch(table, &req_frame, &resp_frame, &has_resp);
    if (dispatch_st != FW_OK) {
        return dispatch_st;
    }

    /* Encode reply into tx buffer if generated */
    if (has_resp && tx_out_buf.data != FW_NULL && out_tx_len != FW_NULL) {
        *out_tx_len = fw_zerowire_encode(&resp_frame, tx_out_buf);
    }

    return FW_OK;
}
