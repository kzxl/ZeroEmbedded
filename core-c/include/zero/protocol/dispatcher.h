#ifndef ZERO_PROTOCOL_DISPATCHER_H
#define ZERO_PROTOCOL_DISPATCHER_H

/**
 * @file dispatcher.h
 * @brief Zero-allocation binary Command & RPC Dispatcher for ZeroWire protocol.
 *
 * Implements deterministic Request-Response routing without heap allocation.
 */

#include "../types.h"
#include "../result.h"
#include "../span.h"
#include "zerowire.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RPC Command Handler Signature.
 * @param seq Sequence number of the incoming request.
 * @param req_payload Immutable span containing request payload bytes.
 * @param resp_buf Mutable buffer for handler to write reply payload (up to ZEROWIRE_MAX_PAYLOAD).
 * @param out_resp_len Pointer to set the number of response payload bytes written (0 if no response).
 * @return FW_OK on success, or error code on validation failure.
 */
typedef fw_status_t (*fw_cmd_handler_t)(
    uint8_t seq,
    fw_cspan_t req_payload,
    fw_span_t resp_buf,
    fw_size_t *out_resp_len
);

typedef struct {
    uint8_t           msg_id;
    uint16_t          min_payload_len;
    fw_cmd_handler_t  handler;
} fw_cmd_entry_t;

typedef struct {
    const fw_cmd_entry_t *entries;
    fw_size_t             entry_count;
} fw_cmd_table_t;

/**
 * @brief Initializes a command dispatch table with a static array of entries.
 */
fw_status_t fw_cmd_table_init(
    fw_cmd_table_t *table,
    const fw_cmd_entry_t *entries,
    fw_size_t entry_count
);

/**
 * @brief Dispatches a decoded ZeroWire request frame, generating an optional response frame.
 * @param table Pointer to initialized command table.
 * @param req Decoded input request frame.
 * @param out_resp Optional destination for response frame (can be FW_NULL if responses unneeded).
 * @param out_has_response Set to FW_TRUE if a response frame was generated.
 * @return FW_OK on success, FW_ERR_NOT_FOUND if MsgID unhandled.
 */
fw_status_t fw_cmd_dispatch(
    const fw_cmd_table_t *table,
    const fw_zerowire_frame_t *req,
    fw_zerowire_frame_t *out_resp,
    fw_bool_t *out_has_response
);

/**
 * @brief High-level helper: Processes raw UART stream slice, executes RPC, and encodes reply.
 * @param table Command table.
 * @param rx_stream Incoming raw UART slice.
 * @param out_consumed Number of bytes consumed from rx_stream.
 * @param tx_out_buf Buffer to receive encoded binary response frame (if any).
 * @param out_tx_len Number of bytes written to tx_out_buf.
 */
fw_status_t fw_cmd_process_stream(
    const fw_cmd_table_t *table,
    fw_cspan_t rx_stream,
    fw_size_t *out_consumed,
    fw_span_t tx_out_buf,
    fw_size_t *out_tx_len
);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_PROTOCOL_DISPATCHER_H */
