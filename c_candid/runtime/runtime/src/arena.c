#include "idl/arena.h"
#include "idl/cdk_alloc.h"

#include <stdlib.h>
#include <string.h>

static size_t align_size(size_t size) {
    const size_t align = sizeof(void *);
    size_t       rem = size % align;
    if (rem == 0) {
        return size;
    }
    return size + (align - rem);
}

static idl_status idl_arena_add_block(idl_arena *arena, size_t min_size) {
    size_t block_size = arena->default_block_size;
    if (block_size < min_size) {
        block_size = min_size;
    }
    idl_arena_block *block = (idl_arena_block *)malloc(sizeof(idl_arena_block));
    if (!block) {
        return IDL_STATUS_ERR_ALLOC;
    }
    block->data = (uint8_t *)malloc(block_size);
    if (!block->data) {
        free(block);
        return IDL_STATUS_ERR_ALLOC;
    }
    block->capacity = block_size;
    block->used = 0;
    block->next = arena->head;
    arena->head = block;
    return IDL_STATUS_OK;
}

idl_status idl_arena_init(idl_arena *arena, size_t default_block_size) {
    if (!arena) {
        return IDL_STATUS_ERR_INVALID_ARG;
    }
    if (default_block_size == 0) {
        default_block_size = 4096;
    }
    arena->head = NULL;
    arena->default_block_size = default_block_size;
    return IDL_STATUS_OK;
}

void idl_arena_reset(idl_arena *arena) {
    if (!arena) {
        return;
    }
    idl_arena_block *block = arena->head;
    while (block) {
        block->used = 0;
        block = block->next;
    }
}

void idl_arena_destroy(idl_arena *arena) {
    if (!arena) {
        return;
    }
    idl_arena_block *block = arena->head;
    while (block) {
        idl_arena_block *next = block->next;
        free(block->data);
        free(block);
        block = next;
    }
    arena->head = NULL;
}

static void *arena_alloc_internal(idl_arena *arena, size_t size, int zero) {
    if (!arena || size == 0) {
        return NULL;
    }
    size = align_size(size);
    if (!arena->head || (arena->head->capacity - arena->head->used) < size) {
        if (idl_arena_add_block(arena, size) != IDL_STATUS_OK) {
            return NULL;
        }
    }
    idl_arena_block *block = arena->head;
    void            *ptr = block->data + block->used;
    block->used += size;
    if (zero) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void *idl_arena_alloc(idl_arena *arena, size_t size) {
    return arena_alloc_internal(arena, size, 0);
}

void *idl_arena_alloc_zeroed(idl_arena *arena, size_t size) {
    return arena_alloc_internal(arena, size, 1);
}

void *idl_arena_dup(idl_arena *arena, const void *data, size_t size) {
    if (data == NULL || size == 0) {
        return NULL;
    }
    void *dest = idl_arena_alloc(arena, size);
    if (!dest) {
        return NULL;
    }
    memcpy(dest, data, size);
    return dest;
}
