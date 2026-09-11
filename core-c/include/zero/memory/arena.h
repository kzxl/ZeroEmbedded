#ifndef ZERO_MEMORY_ARENA_H
#define ZERO_MEMORY_ARENA_H

/**
 * @file arena.h
 * @brief Linear bump allocator (Arena) with scoped rewind support for ZeroEmbedded.
 *
 * Fast O(1) allocation with zero per-allocation overhead. Ideal for frame-scoped
 * buffers, protocol packet parsing, and initialization phases.
 */

#include "../types.h"
#include "../result.h"
#include "../span.h"
#include "../attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef fw_size_t fw_arena_mark_t;

typedef struct {
    uint8_t    *buffer;
    fw_size_t   capacity;
    fw_size_t   offset;
    fw_size_t   peak_used;
} fw_arena_t;

/**
 * @brief Initializes a linear arena backed by pre-allocated memory.
 *
 * @param arena Pointer to arena control structure.
 * @param buffer Raw buffer for arena memory.
 * @param buffer_size Capacity of buffer in bytes.
 * @return FW_OK on success, or error code on invalid parameters.
 */
fw_status_t fw_arena_init(fw_arena_t *arena, void *buffer, fw_size_t buffer_size);

/**
 * @brief Allocates a block of memory from the arena.
 *
 * @param arena Pointer to initialized arena.
 * @param size Requested allocation size in bytes.
 * @param alignment Required alignment (must be power of 2: 1, 2, 4, 8, etc.).
 * @return Pointer to allocated memory, or FW_NULL if capacity exceeded.
 */
FW_NODISCARD FW_OWNER void* fw_arena_alloc(
    fw_arena_t *arena,
    fw_size_t size,
    fw_size_t alignment
);

/**
 * @brief Allocates a memory span directly from the arena.
 */
FW_NODISCARD fw_span_t fw_arena_alloc_span(
    fw_arena_t *arena,
    fw_size_t size,
    fw_size_t alignment
);

/**
 * @brief Allocates a zero-initialized block of memory from the arena.
 */
FW_NODISCARD FW_OWNER void* fw_arena_alloc_zeroed(
    fw_arena_t *arena,
    fw_size_t size,
    fw_size_t alignment
);

/**
 * @brief Saves current arena allocation mark for scoped rewind.
 */
FW_INLINE fw_arena_mark_t fw_arena_mark(const fw_arena_t *arena) {
    return arena != FW_NULL ? arena->offset : 0;
}

/**
 * @brief Rewinds the arena to a previously saved mark, releasing subsequent allocations.
 */
void fw_arena_rewind(fw_arena_t *arena, fw_arena_mark_t mark);

/**
 * @brief Resets the entire arena back to zero offset.
 */
void fw_arena_reset(fw_arena_t *arena);

/** Returns number of bytes currently allocated */
FW_INLINE fw_size_t fw_arena_used(const fw_arena_t *arena) {
    return arena != FW_NULL ? arena->offset : 0;
}

/** Returns remaining available bytes in arena */
FW_INLINE fw_size_t fw_arena_available(const fw_arena_t *arena) {
    return (arena != FW_NULL && arena->capacity >= arena->offset) ?
           (arena->capacity - arena->offset) : 0;
}

/** Returns peak memory usage recorded in this arena */
FW_INLINE fw_size_t fw_arena_peak(const fw_arena_t *arena) {
    return arena != FW_NULL ? arena->peak_used : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* ZERO_MEMORY_ARENA_H */
