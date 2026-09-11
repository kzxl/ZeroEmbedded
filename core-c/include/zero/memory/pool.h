#ifndef ZERO_MEMORY_POOL_H
#define ZERO_MEMORY_POOL_H

/**
 * @file pool.h
 * @brief Fixed-size block memory pool allocator for ZeroEmbedded.
 *
 * Provides deterministic O(1) allocation and deallocation with zero
 * fragmentation, suitable for ISR and real-time execution contexts.
 */

#include "../types.h"
#include "../result.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fw_pool_node {
    struct fw_pool_node *next;
} fw_pool_node_t;

typedef struct {
    void            *storage;
    fw_size_t        storage_size;
    fw_size_t        block_size;
    fw_size_t        alignment;
    fw_size_t        capacity;
    fw_size_t        free_count;
    fw_pool_node_t  *free_list;
    uint32_t        *alloc_bitmap;
    uint8_t          block_shift; /* Log2 of block_size if power of 2, else 0 */
} fw_pool_t;

/**
 * @brief Initializes a memory pool backed by pre-allocated memory.
 *
 * @param pool Pointer to pool control structure.
 * @param buffer Contiguous memory buffer provided for the pool.
 * @param buffer_size Total size of buffer in bytes.
 * @param block_size Size of each allocatable block in bytes.
 * @param alignment Required alignment in bytes (must be power of 2, e.g. 4, 8, 16).
 * @return FW_OK on success, or error code on invalid configuration.
 */
fw_status_t fw_pool_init(
    fw_pool_t *pool,
    void *buffer,
    fw_size_t buffer_size,
    fw_size_t block_size,
    fw_size_t alignment
);

/**
 * @brief Allocates one fixed-size block from the pool in O(1) time.
 *
 * @param pool Pointer to initialized pool.
 * @return Pointer to allocated block, or FW_NULL if pool is exhausted.
 */
FW_NODISCARD FW_OWNER void* fw_pool_alloc(fw_pool_t *pool);

/**
 * @brief Returns a previously allocated block back to the pool in O(1) time.
 *
 * @param pool Pointer to initialized pool.
 * @param block Pointer to block to release.
 * @return FW_OK on success, or error code if block does not belong to pool.
 */
fw_status_t fw_pool_free(fw_pool_t *pool, void *block);

/** Returns number of currently available blocks */
FW_INLINE fw_size_t fw_pool_available(const fw_pool_t *pool) {
    return pool != FW_NULL ? pool->free_count : 0;
}

/** Returns total capacity in blocks */
FW_INLINE fw_size_t fw_pool_capacity(const fw_pool_t *pool) {
    return pool != FW_NULL ? pool->capacity : 0;
}

/** Returns true if pool is completely full (no free blocks) */
FW_INLINE fw_bool_t fw_pool_is_exhausted(const fw_pool_t *pool) {
    return pool != FW_NULL ? (pool->free_count == 0) : FW_TRUE;
}

#ifdef __cplusplus
}
#endif

#endif /* ZERO_MEMORY_POOL_H */
