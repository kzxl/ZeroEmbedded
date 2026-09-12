#include "zero/memory/hub.h"
#include <string.h>

fw_status_t fw_mem_hub_init(fw_mem_hub_t *hub) {
    if (hub == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    memset(hub, 0, sizeof(*hub));
    return FW_OK;
}

fw_status_t fw_mem_hub_register_pool(
    fw_mem_hub_t *hub,
    fw_mem_tier_t tier,
    fw_pool_t    *pool,
    const char   *name,
    fw_size_t     reserve_bytes
) {
    if (hub == FW_NULL || pool == FW_NULL || tier >= FW_MEM_TIER_COUNT) {
        return FW_ERR_INVALID_ARG;
    }
    if (hub->backend_count >= FW_MEM_HUB_MAX_BACKENDS) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    fw_mem_backend_t *b = &hub->backends[hub->backend_count++];
    b->name = name ? name : "unnamed_pool";
    b->tier = tier;
    b->type = FW_BACKEND_POOL;
    b->handle.pool = pool;
    b->reserve_bytes = reserve_bytes;
    b->allocated_bytes = 0;
    b->peak_used_bytes = 0;

    return FW_OK;
}

fw_status_t fw_mem_hub_register_arena(
    fw_mem_hub_t *hub,
    fw_mem_tier_t tier,
    fw_arena_t   *arena,
    const char   *name,
    fw_size_t     reserve_bytes
) {
    if (hub == FW_NULL || arena == FW_NULL || tier >= FW_MEM_TIER_COUNT) {
        return FW_ERR_INVALID_ARG;
    }
    if (hub->backend_count >= FW_MEM_HUB_MAX_BACKENDS) {
        return FW_ERR_BUFFER_OVERFLOW;
    }

    fw_mem_backend_t *b = &hub->backends[hub->backend_count++];
    b->name = name ? name : "unnamed_arena";
    b->tier = tier;
    b->type = FW_BACKEND_ARENA;
    b->handle.arena = arena;
    b->reserve_bytes = reserve_bytes;
    b->allocated_bytes = 0;
    b->peak_used_bytes = 0;

    return FW_OK;
}

static void* try_alloc_from_backend(fw_mem_backend_t *b, fw_size_t size) {
    if (b->type == FW_BACKEND_POOL) {
        fw_pool_t *pool = b->handle.pool;
        if (size > pool->block_size) {
            return FW_NULL; /* Pool block too small */
        }
        fw_size_t free_bytes = pool->free_count * pool->block_size;
        if (free_bytes <= b->reserve_bytes || (free_bytes - pool->block_size) < b->reserve_bytes) {
            return FW_NULL; /* Safety margin breach */
        }

        void *ptr = fw_pool_alloc(pool);
        if (ptr != FW_NULL) {
            b->allocated_bytes += pool->block_size;
            if (b->allocated_bytes > b->peak_used_bytes) {
                b->peak_used_bytes = b->allocated_bytes;
            }
            return ptr;
        }
    } else if (b->type == FW_BACKEND_ARENA) {
        fw_arena_t *arena = b->handle.arena;
        fw_size_t free_bytes = (arena->capacity > arena->offset) ? (arena->capacity - arena->offset) : 0;
        if (free_bytes <= b->reserve_bytes || size > free_bytes || (free_bytes - size) < b->reserve_bytes) {
            return FW_NULL; /* Safety margin breach */
        }

        void *ptr = fw_arena_alloc(arena, size, 4);
        if (ptr != FW_NULL) {
            b->allocated_bytes += size;
            if (b->allocated_bytes > b->peak_used_bytes) {
                b->peak_used_bytes = b->allocated_bytes;
            }
            return ptr;
        }
    }
    return FW_NULL;
}

void* fw_mem_hub_alloc(fw_mem_hub_t *hub, fw_size_t size, uint32_t hints) {
    if (hub == FW_NULL || size == 0) {
        return FW_NULL;
    }

    hub->total_alloc_ops++;

    fw_mem_tier_t search_order[FW_MEM_TIER_COUNT];
    fw_size_t order_len = 0;

    if (hints & FW_MEM_HINT_DMA) {
        search_order[order_len++] = FW_MEM_TIER_DMA;
        /* DMA requests typically require coherent memory; do not fall back to non-DMA unless relaxed */
    } else if (hints & FW_MEM_HINT_FAST) {
        search_order[order_len++] = FW_MEM_TIER_FAST;
        search_order[order_len++] = FW_MEM_TIER_BULK;
        search_order[order_len++] = FW_MEM_TIER_DMA;
    } else {
        search_order[order_len++] = FW_MEM_TIER_BULK;
        search_order[order_len++] = FW_MEM_TIER_FAST;
        search_order[order_len++] = FW_MEM_TIER_DMA;
    }

    /* Try backends matching preferred tier order */
    for (fw_size_t i = 0; i < order_len; i++) {
        fw_mem_tier_t target_tier = search_order[i];
        for (fw_size_t b_idx = 0; b_idx < hub->backend_count; b_idx++) {
            fw_mem_backend_t *b = &hub->backends[b_idx];
            if (b->tier == target_tier) {
                void *ptr = try_alloc_from_backend(b, size);
                if (ptr != FW_NULL) {
                    return ptr;
                }
            }
        }
    }

    hub->total_rejected_ops++;
    return FW_NULL;
}

fw_status_t fw_mem_hub_free(fw_mem_hub_t *hub, void *ptr) {
    if (hub == FW_NULL || ptr == FW_NULL) {
        return FW_ERR_INVALID_ARG;
    }

    for (fw_size_t i = 0; i < hub->backend_count; i++) {
        fw_mem_backend_t *b = &hub->backends[i];
        if (b->type == FW_BACKEND_POOL) {
            fw_pool_t *pool = b->handle.pool;
            uintptr_t p = (uintptr_t)ptr;
            uintptr_t start = (uintptr_t)pool->storage;
            uintptr_t end = start + pool->storage_size;
            if (p >= start && p < end) {
                fw_status_t res = fw_pool_free(pool, ptr);
                if (res == FW_OK && b->allocated_bytes >= pool->block_size) {
                    b->allocated_bytes -= pool->block_size;
                }
                return res;
            }
        }
    }

    return FW_ERR_NOT_FOUND;
}

fw_size_t fw_mem_hub_get_free_space(const fw_mem_hub_t *hub, fw_mem_tier_t tier) {
    if (hub == FW_NULL || tier >= FW_MEM_TIER_COUNT) {
        return 0;
    }

    fw_size_t total_free = 0;
    for (fw_size_t i = 0; i < hub->backend_count; i++) {
        const fw_mem_backend_t *b = &hub->backends[i];
        if (b->tier == tier) {
            if (b->type == FW_BACKEND_POOL) {
                fw_size_t raw_free = b->handle.pool->free_count * b->handle.pool->block_size;
                if (raw_free > b->reserve_bytes) {
                    total_free += (raw_free - b->reserve_bytes);
                }
            } else if (b->type == FW_BACKEND_ARENA) {
                fw_arena_t *a = b->handle.arena;
                fw_size_t raw_free = (a->capacity > a->offset) ? (a->capacity - a->offset) : 0;
                if (raw_free > b->reserve_bytes) {
                    total_free += (raw_free - b->reserve_bytes);
                }
            }
        }
    }
    return total_free;
}

fw_size_t fw_mem_hub_get_total_used(const fw_mem_hub_t *hub) {
    if (hub == FW_NULL) {
        return 0;
    }

    fw_size_t used = 0;
    for (fw_size_t i = 0; i < hub->backend_count; i++) {
        used += hub->backends[i].allocated_bytes;
    }
    return used;
}
