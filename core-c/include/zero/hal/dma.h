#ifndef ZERO_HAL_DMA_H
#define ZERO_HAL_DMA_H

/**
 * @file dma.h
 * @brief Memory-to-Memory DMA Coprocessor & 2D Strided Transfer Engine.
 * Inspired by Magic Lantern's EDMAC memory acceleration framework.
 */

#include "../types.h"
#include "../result.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*fw_dma_complete_cb_t)(void *context);

typedef struct {
    uint32_t total_transfers;
    uint32_t total_bytes;
    uint32_t total_2d_transfers;
} fw_dma_stats_t;

/**
 * @brief High-speed synchronous Memory-to-Memory block copy.
 * Employs word-aligned burst transfers with hardware barriers.
 */
fw_status_t fw_dma_memcpy(void *dst, const void *src, fw_size_t length);

/**
 * @brief Synchronous Memory-to-Memory fill (memset).
 */
fw_status_t fw_dma_memset(void *dst, uint8_t value, fw_size_t length);

/**
 * @brief 2D rectangular strided block transfer without CPU pixel overhead.
 * Ideal for cropping sensor RAW regions, spectrogram buffers, or LCD frame updates.
 *
 * @param dst Target buffer pointer for top-left corner.
 * @param dst_stride Row byte-pitch of destination buffer.
 * @param src Source buffer pointer for top-left corner.
 * @param src_stride Row byte-pitch of source buffer.
 * @param width_bytes Width of rectangular region to copy per line in bytes.
 * @param height_lines Number of lines to transfer.
 */
fw_status_t fw_dma_copy_2d(
    void       *dst,
    fw_size_t   dst_stride,
    const void *src,
    fw_size_t   src_stride,
    fw_size_t   width_bytes,
    fw_size_t   height_lines
);

/**
 * @brief Queries global DMA coprocessor transfer statistics.
 */
void fw_dma_get_stats(fw_dma_stats_t *out_stats);

/**
 * @brief Resets DMA statistics counter.
 */
void fw_dma_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_HAL_DMA_H */
