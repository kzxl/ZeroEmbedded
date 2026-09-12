#ifndef ZERO_MEMORY_HUB_H
#define ZERO_MEMORY_HUB_H

/**
 * @file hub.h
 * @brief Multi-Tier Memory Hub Router & Safety Governor.
 * Inspired by Magic Lantern's multi-backend mem_allocator architecture.
 */

#include "../types.h"
#include "../result.h"
#include "../attributes.h"
#include "pool.h"
#include "arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FW_MEM_TIER_FAST = 0, /**< Internal SRAM / TCM / DTCM */
    FW_MEM_TIER_DMA  = 1, /**< Coherent / Uncached RAM for DMA */
    FW_MEM_TIER_BULK = 2, /**< External SDRAM / PSRAM / Bulk Heap */
    FW_MEM_TIER_COUNT = 3
} fw_mem_tier_t;

#define FW_MEM_HINT_FAST      (1U << 0)
#define FW_MEM_HINT_DMA       (1U << 1)
#define FW_MEM_HINT_BULK      (1U << 2)
#define FW_MEM_HINT_TEMPORARY (1U << 3)

#define FW_MEM_HUB_MAX_BACKENDS 8

typedef enum {
    FW_BACKEND_NONE  = 0,
    FW_BACKEND_POOL  = 1,
    FW_BACKEND_ARENA = 2
} fw_mem_backend_type_t;

typedef struct {
    const char           *name;
    fw_mem_tier_t         tier;
    fw_mem_backend_type_t type;
    union {
        fw_pool_t  *pool;
        fw_arena_t *arena;
    } handle;
    fw_size_t             reserve_bytes;
    fw_size_t             allocated_bytes;
    fw_size_t             peak_used_bytes;
} fw_mem_backend_t;

typedef struct {
    fw_mem_backend_t backends[FW_MEM_HUB_MAX_BACKENDS];
    fw_size_t        backend_count;
    fw_size_t        total_alloc_ops;
    fw_size_t        total_rejected_ops;
} fw_mem_hub_t;

/**
 * @brief Initializes a multi-tier memory hub router.
 */
fw_status_t fw_mem_hub_init(fw_mem_hub_t *hub);

/**
 * @brief Registers an O(1) bitmap pool into the memory hub.
 */
fw_status_t fw_mem_hub_register_pool(
    fw_mem_hub_t *hub,
    fw_mem_tier_t tier,
    fw_pool_t    *pool,
    const char   *name,
    fw_size_t     reserve_bytes
);

/**
 * @brief Registers a linear arena allocator into the memory hub.
 */
fw_status_t fw_mem_hub_register_arena(
    fw_mem_hub_t *hub,
    fw_mem_tier_t tier,
    fw_arena_t   *arena,
    const char   *name,
    fw_size_t     reserve_bytes
);

/**
 * @brief Allocates memory with context hints and safety margin enforcement.
 * If the preferred tier does not have enough headroom above reserve_bytes,
 * automatically falls back to alternative tiers.
 */
void* fw_mem_hub_alloc(fw_mem_hub_t *hub, fw_size_t size, uint32_t hints);

/**
 * @brief Frees a previously allocated buffer back to its owning pool.
 */
fw_status_t fw_mem_hub_free(fw_mem_hub_t *hub, void *ptr);

/**
 * @brief Queries total free bytes in a specific memory tier above reserve threshold.
 */
fw_size_t fw_mem_hub_get_free_space(const fw_mem_hub_t *hub, fw_mem_tier_t tier);

/**
 * @brief Queries total active allocated bytes across all tiers.
 */
fw_size_t fw_mem_hub_get_total_used(const fw_mem_hub_t *hub);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_MEMORY_HUB_H */
