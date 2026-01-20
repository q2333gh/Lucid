// Shim API for native mock + IC libc subset.
// Provides blob, time, random, and logging hooks with injectable backends.
#pragma once

#include <stddef.h>
#include <stdint.h>

// Result codes for shim operations.
typedef enum {
    SHIM_OK = 0,
    SHIM_ERR_INVALID_ARG = 1,
    SHIM_ERR_NOT_FOUND = 2,
    SHIM_ERR_OUT_OF_MEMORY = 3,
    SHIM_ERR_OUT_OF_BOUNDS = 4,
    SHIM_ERR_IO = 5,
    SHIM_ERR_UNSUPPORTED = 6,
} shim_result_t;

// Mapping ownership semantics for shim_map.
typedef enum {
    SHIM_MAP_BORROWED = 0,
    SHIM_MAP_OWNED = 1,
    SHIM_MAP_MAPPED = 2,
} shim_map_kind_t;

// Blob mapping result.
typedef struct {
    const uint8_t  *data;
    size_t          len;
    shim_map_kind_t kind;
    void           *handle;
} shim_map_t;

// Shim backend function table.
typedef struct {
    void *ctx;

    shim_result_t (*blob_size)(void *ctx, const char *name, size_t *out_size);
    shim_result_t (*blob_read)(
        void *ctx, const char *name, size_t offset, uint8_t *dst, size_t len);
    shim_result_t (*map)(void *ctx, const char *name, shim_map_t *out_map);
    void (*unmap)(void *ctx, shim_map_t *map);

    void (*log)(void *ctx, const char *msg);
    uint64_t (*time_ns)(void *ctx);
    shim_result_t (*getrandom)(void *ctx, uint8_t *dst, size_t len);
} shim_ops_t;

// Configure shim operations (inject custom backend).
void              shim_set_ops(const shim_ops_t *ops);
void              shim_reset_ops(void);
const shim_ops_t *shim_get_ops(void);

// Convenience wrappers using the configured backend.
shim_result_t shim_blob_size(const char *name, size_t *out_size);
shim_result_t
shim_blob_read(const char *name, size_t offset, uint8_t *dst, size_t len);
shim_result_t shim_map(const char *name, shim_map_t *out_map);
void          shim_unmap(shim_map_t *map);

void          shim_log(const char *msg);
uint64_t      shim_time_ns(void);
shim_result_t shim_getrandom(uint8_t *dst, size_t len);

// Blob registry for IC stable memory backends.
// Registering a name allows shim_* calls to resolve stable offsets.
shim_result_t shim_register_blob(const char *name, int64_t offset, size_t len);
shim_result_t shim_unregister_blob(const char *name);
void          shim_clear_blobs(void);
shim_result_t
shim_lookup_blob(const char *name, int64_t *out_offset, size_t *out_len);
