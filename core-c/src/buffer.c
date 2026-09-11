#include "zero/memory/buffer.h"
#include <string.h>

fw_status_t fw_buffer_init(fw_buffer_t *buf, void *storage, fw_size_t capacity) {
    if (buf == FW_NULL || storage == FW_NULL || capacity == 0) {
        return FW_ERR_INVALID_ARG;
    }

    buf->data = (uint8_t*)storage;
    buf->capacity = capacity;
    buf->length = 0;

    return FW_OK;
}

fw_status_t fw_buffer_append(fw_buffer_t *buf, const void *src, fw_size_t count) {
    if (buf == FW_NULL || src == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }
    if (count == 0) {
        return FW_OK;
    }

    if (count > buf->capacity - buf->length) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    memcpy(buf->data + buf->length, src, count);
    buf->length += count;

    return FW_OK;
}

fw_status_t fw_buffer_append_byte(fw_buffer_t *buf, uint8_t byte) {
    if (buf == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    if (buf->length >= buf->capacity) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    buf->data[buf->length++] = byte;
    return FW_OK;
}
