#include "zero/memory/arena.h"

FW_INLINE uintptr_t align_up(uintptr_t ptr, fw_size_t align) {
    return (ptr + (align - 1)) & ~(uintptr_t)(align - 1);
}

fw_status_t fw_arena_init(fw_arena_t *arena, void *buffer, fw_size_t buffer_size) {
    if (arena == FW_NULL || buffer == FW_NULL || buffer_size == 0) {
        return FW_ERR_INVALID_ARG;
    }

    arena->buffer = (uint8_t*)buffer;
    arena->capacity = buffer_size;
    arena->offset = 0;
    arena->peak_used = 0;

    return FW_OK;
}

void* fw_arena_alloc(fw_arena_t *arena, fw_size_t size, fw_size_t alignment) {
    if (arena == FW_NULL || size == 0) {
        return FW_NULL;
    }

    if (alignment == 0) {
        alignment = sizeof(void*);
    }

    uintptr_t curr_ptr = (uintptr_t)(arena->buffer + arena->offset);
    uintptr_t aligned_ptr = align_up(curr_ptr, alignment);
    fw_size_t padding = (fw_size_t)(aligned_ptr - curr_ptr);

    if (arena->offset + padding + size > arena->capacity) {
        return FW_NULL; /* Out of memory */
    }

    arena->offset += (padding + size);
    if (arena->offset > arena->peak_used) {
        arena->peak_used = arena->offset;
    }

    return (void*)aligned_ptr;
}

fw_span_t fw_arena_alloc_span(fw_arena_t *arena, fw_size_t size, fw_size_t alignment) {
    void *ptr = fw_arena_alloc(arena, size, alignment);
    if (ptr == FW_NULL) {
        return fw_span_make(FW_NULL, 0);
    }
    return fw_span_make(ptr, size);
}

void fw_arena_rewind(fw_arena_t *arena, fw_arena_mark_t mark) {
    if (arena != FW_NULL && mark <= arena->offset) {
        arena->offset = mark;
    }
}

void fw_arena_reset(fw_arena_t *arena) {
    if (arena != FW_NULL) {
        arena->offset = 0;
    }
}
