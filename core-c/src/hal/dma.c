#include "zero/hal/dma.h"
#include <string.h>

static fw_dma_stats_t s_dma_stats = {0, 0, 0};

fw_status_t fw_dma_memcpy(void *dst, const void *src, fw_size_t length) {
    if (dst == FW_NULL || src == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }
    if (length == 0) {
        return FW_OK;
    }

    FW_MEMORY_BARRIER();

    /* 32-bit / 64-bit word burst transfer optimization when pointers are aligned */
    uintptr_t d_addr = (uintptr_t)dst;
    uintptr_t s_addr = (uintptr_t)src;

    if (((d_addr | s_addr) & 0x3) == 0 && (length & 0x3) == 0) {
        uint32_t *d32 = (uint32_t*)dst;
        const uint32_t *s32 = (const uint32_t*)src;
        fw_size_t words = length >> 2;
        for (fw_size_t i = 0; i < words; i++) {
            d32[i] = s32[i];
        }
    } else {
        memcpy(dst, src, length);
    }

    FW_MEMORY_BARRIER();

    s_dma_stats.total_transfers++;
    s_dma_stats.total_bytes += (uint32_t)length;

    return FW_OK;
}

fw_status_t fw_dma_memset(void *dst, uint8_t value, fw_size_t length) {
    if (dst == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }
    if (length == 0) {
        return FW_OK;
    }

    FW_MEMORY_BARRIER();
    memset(dst, (int)value, length);
    FW_MEMORY_BARRIER();

    s_dma_stats.total_transfers++;
    s_dma_stats.total_bytes += (uint32_t)length;

    return FW_OK;
}

fw_status_t fw_dma_copy_2d(
    void       *dst,
    fw_size_t   dst_stride,
    const void *src,
    fw_size_t   src_stride,
    fw_size_t   width_bytes,
    fw_size_t   height_lines
) {
    if (dst == FW_NULL || src == FW_NULL || width_bytes == 0 || height_lines == 0) {
        return FW_ERR_INVALID_ARG;
    }
    if (dst_stride < width_bytes || src_stride < width_bytes) {
        return FW_ERR_INVALID_ARG;
    }

    FW_MEMORY_BARRIER();

    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;

    for (fw_size_t line = 0; line < height_lines; line++) {
        memcpy(d + (line * dst_stride), s + (line * src_stride), width_bytes);
    }

    FW_MEMORY_BARRIER();

    s_dma_stats.total_transfers++;
    s_dma_stats.total_2d_transfers++;
    s_dma_stats.total_bytes += (uint32_t)(width_bytes * height_lines);

    return FW_OK;
}

void fw_dma_get_stats(fw_dma_stats_t *out_stats) {
    if (out_stats != FW_NULL) {
        *out_stats = s_dma_stats;
    }
}

void fw_dma_reset_stats(void) {
    memset(&s_dma_stats, 0, sizeof(s_dma_stats));
}
