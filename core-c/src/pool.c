#include "zero/memory/pool.h"
#include "zero/assert.h"
#include <string.h>

FW_INLINE uintptr_t align_up(uintptr_t ptr, fw_size_t align) {
    return (ptr + (align - 1)) & ~(uintptr_t)(align - 1);
}

fw_status_t fw_pool_init(
    fw_pool_t *pool,
    void *buffer,
    fw_size_t buffer_size,
    fw_size_t block_size,
    fw_size_t alignment
) {
    if (pool == FW_NULL || buffer == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    /* Alignment must be power of 2 and at least pointer aligned */
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return FW_ERR_INVALID_ARG;
    }
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }

    /* Minimum block size must fit intrusive node pointer */
    if (block_size < sizeof(fw_pool_node_t)) {
        block_size = sizeof(fw_pool_node_t);
    }

    /* Align block size up to alignment */
    fw_size_t aligned_block_size = (fw_size_t)align_up(block_size, alignment);

    /* Align start of storage buffer */
    uintptr_t raw_start = (uintptr_t)buffer;
    uintptr_t aligned_start = align_up(raw_start, alignment);
    fw_size_t overhead = (fw_size_t)(aligned_start - raw_start);

    if (buffer_size <= overhead) {
        return FW_ERR_INVALID_ARG;
    }

    fw_size_t usable_size = buffer_size - overhead;

    /* Reserve space for allocation bitmap at the beginning of aligned storage */
    fw_size_t estimated_cap = usable_size / aligned_block_size;
    fw_size_t bitmap_words = (estimated_cap + 31) / 32;
    fw_size_t bitmap_bytes = (fw_size_t)align_up(bitmap_words * sizeof(uint32_t), alignment);

    if (usable_size <= bitmap_bytes) {
        return FW_ERR_INVALID_ARG;
    }

    fw_size_t block_space = usable_size - bitmap_bytes;
    fw_size_t capacity = block_space / aligned_block_size;

    if (capacity == 0) {
        return FW_ERR_INVALID_ARG;
    }

    pool->alloc_bitmap = (uint32_t*)aligned_start;
    memset(pool->alloc_bitmap, 0, bitmap_bytes);

    uintptr_t block_start = align_up(aligned_start + bitmap_bytes, alignment);
    pool->storage = (void*)block_start;
    pool->storage_size = capacity * aligned_block_size;
    pool->block_size = aligned_block_size;
    pool->alignment = alignment;
    pool->capacity = capacity;
    pool->free_count = capacity;
    pool->free_list = FW_NULL;

    /* Build intrusive free list linking each block to the next */
    uint8_t *curr = (uint8_t*)pool->storage;
    for (fw_size_t i = 0; i < capacity; ++i) {
        fw_pool_node_t *node = (fw_pool_node_t*)curr;
        node->next = pool->free_list;
        pool->free_list = node;
        curr += aligned_block_size;
    }

    return FW_OK;
}

void* fw_pool_alloc(fw_pool_t *pool) {
    if (pool == FW_NULL || pool->free_list == FW_NULL) {
        return FW_NULL;
    }

    fw_pool_node_t *node = pool->free_list;
    pool->free_list = node->next;
    pool->free_count--;

    fw_size_t idx = (fw_size_t)(((uintptr_t)node - (uintptr_t)pool->storage) / pool->block_size);
    pool->alloc_bitmap[idx / 32] |= (1U << (idx % 32));

    return (void*)node;
}

fw_status_t fw_pool_free(fw_pool_t *pool, void *block) {
    if (pool == FW_NULL || block == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    uintptr_t b_addr = (uintptr_t)block;
    uintptr_t s_addr = (uintptr_t)pool->storage;
    uintptr_t e_addr = s_addr + pool->storage_size;

    /* Range and alignment validation */
    if (b_addr < s_addr || b_addr >= e_addr) {
        return FW_ERR_INVALID_ARG;
    }

    fw_size_t offset = (fw_size_t)(b_addr - s_addr);
    if ((offset % pool->block_size) != 0) {
        return FW_ERR_INVALID_ARG;
    }

    fw_size_t idx = offset / pool->block_size;
    uint32_t mask = 1U << (idx % 32);

    /* Double-Free Check: block must be currently marked as allocated */
    if ((pool->alloc_bitmap[idx / 32] & mask) == 0) {
        return FW_ERR_INVALID_ARG; /* REJECT DOUBLE FREE */
    }

    /* Mark block as free in bitmap */
    pool->alloc_bitmap[idx / 32] &= ~mask;

#if ZERO_ENABLE_ASSERT
    /* Poison memory in debug builds to immediately catch Use-After-Free */
    memset(block, 0xDF, pool->block_size);
#endif

    /* Push back onto intrusive free list */
    fw_pool_node_t *node = (fw_pool_node_t*)block;
    node->next = pool->free_list;
    pool->free_list = node;
    pool->free_count++;

    return FW_OK;
}
