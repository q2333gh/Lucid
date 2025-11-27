#ifndef IDL_ARENA_H
#define IDL_ARENA_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct idl_arena_block {
    uint8_t *data;
    size_t capacity;
    size_t used;
    struct idl_arena_block *next;
} idl_arena_block;

typedef struct idl_arena {
    idl_arena_block *head;
    size_t default_block_size;
} idl_arena;

idl_status idl_arena_init(idl_arena *arena, size_t default_block_size);
void idl_arena_reset(idl_arena *arena);
void idl_arena_destroy(idl_arena *arena);
void *idl_arena_alloc(idl_arena *arena, size_t size);
void *idl_arena_alloc_zeroed(idl_arena *arena, size_t size);
void *idl_arena_dup(idl_arena *arena, const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* IDL_ARENA_H */


