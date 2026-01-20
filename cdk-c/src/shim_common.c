// Shared shim dispatch and default routing.
#include "shim/shim.h"

#include "idl/cdk_alloc.h"

#include <limits.h>
#include <string.h>

const shim_ops_t *shim_native_default_ops(void);
const shim_ops_t *shim_ic_default_ops(void);

static const shim_ops_t *g_ops = NULL;

typedef struct {
    char   *name;
    int64_t offset;
    size_t  len;
} shim_blob_entry_t;

static shim_blob_entry_t g_blobs[64];

static const shim_ops_t *shim_default_ops(void) {
#if defined(__wasm32__) || defined(__wasm32_wasi__) || defined(__wasm__)
    return shim_ic_default_ops();
#else
    return shim_native_default_ops();
#endif
}

const shim_ops_t *shim_get_ops(void) {
    if (g_ops == NULL) {
        g_ops = shim_default_ops();
    }
    return g_ops;
}

void shim_set_ops(const shim_ops_t *ops) { g_ops = ops; }

void shim_reset_ops(void) { g_ops = NULL; }

shim_result_t shim_blob_size(const char *name, size_t *out_size) {
    const shim_ops_t *ops = shim_get_ops();
    if (ops == NULL || ops->blob_size == NULL) {
        return SHIM_ERR_UNSUPPORTED;
    }
    return ops->blob_size(ops->ctx, name, out_size);
}

shim_result_t
shim_blob_read(const char *name, size_t offset, uint8_t *dst, size_t len) {
    const shim_ops_t *ops = shim_get_ops();
    if (ops == NULL || ops->blob_read == NULL) {
        return SHIM_ERR_UNSUPPORTED;
    }
    return ops->blob_read(ops->ctx, name, offset, dst, len);
}

shim_result_t shim_map(const char *name, shim_map_t *out_map) {
    const shim_ops_t *ops = shim_get_ops();
    if (ops == NULL || ops->map == NULL) {
        return SHIM_ERR_UNSUPPORTED;
    }
    return ops->map(ops->ctx, name, out_map);
}

void shim_unmap(shim_map_t *map) {
    const shim_ops_t *ops = shim_get_ops();
    if (map == NULL) {
        return;
    }
    if (ops != NULL && ops->unmap != NULL) {
        ops->unmap(ops->ctx, map);
    } else if (map->kind == SHIM_MAP_OWNED && map->data != NULL) {
        cdk_free((void *)map->data);
    }
    map->data = NULL;
    map->len = 0;
    map->kind = SHIM_MAP_BORROWED;
    map->handle = NULL;
}

void shim_log(const char *msg) {
    const shim_ops_t *ops = shim_get_ops();
    if (ops != NULL && ops->log != NULL) {
        ops->log(ops->ctx, msg);
    }
}

uint64_t shim_time_ns(void) {
    const shim_ops_t *ops = shim_get_ops();
    if (ops != NULL && ops->time_ns != NULL) {
        return ops->time_ns(ops->ctx);
    }
    return 0;
}

shim_result_t shim_getrandom(uint8_t *dst, size_t len) {
    const shim_ops_t *ops = shim_get_ops();
    if (ops == NULL || ops->getrandom == NULL) {
        return SHIM_ERR_UNSUPPORTED;
    }
    return ops->getrandom(ops->ctx, dst, len);
}

static shim_blob_entry_t *shim_find_blob(const char *name) {
    if (name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(g_blobs) / sizeof(g_blobs[0]); i++) {
        if (g_blobs[i].name != NULL && strcmp(g_blobs[i].name, name) == 0) {
            return &g_blobs[i];
        }
    }
    return NULL;
}

shim_result_t shim_register_blob(const char *name, int64_t offset, size_t len) {
    if (name == NULL || name[0] == '\0' || offset < 0) {
        return SHIM_ERR_INVALID_ARG;
    }

    shim_blob_entry_t *entry = shim_find_blob(name);
    if (entry != NULL) {
        entry->offset = offset;
        entry->len = len;
        return SHIM_OK;
    }

    for (size_t i = 0; i < sizeof(g_blobs) / sizeof(g_blobs[0]); i++) {
        if (g_blobs[i].name == NULL) {
            size_t name_len = strlen(name);
            char  *copy = (char *)cdk_malloc(name_len + 1);
            if (copy == NULL) {
                return SHIM_ERR_OUT_OF_MEMORY;
            }
            memcpy(copy, name, name_len + 1);
            g_blobs[i].name = copy;
            g_blobs[i].offset = offset;
            g_blobs[i].len = len;
            return SHIM_OK;
        }
    }

    return SHIM_ERR_OUT_OF_MEMORY;
}

shim_result_t shim_unregister_blob(const char *name) {
    shim_blob_entry_t *entry = shim_find_blob(name);
    if (entry == NULL) {
        return SHIM_ERR_NOT_FOUND;
    }
    cdk_free(entry->name);
    entry->name = NULL;
    entry->offset = 0;
    entry->len = 0;
    return SHIM_OK;
}

void shim_clear_blobs(void) {
    for (size_t i = 0; i < sizeof(g_blobs) / sizeof(g_blobs[0]); i++) {
        if (g_blobs[i].name != NULL) {
            cdk_free(g_blobs[i].name);
            g_blobs[i].name = NULL;
        }
        g_blobs[i].offset = 0;
        g_blobs[i].len = 0;
    }
}

shim_result_t
shim_lookup_blob(const char *name, int64_t *out_offset, size_t *out_len) {
    if (out_offset == NULL || out_len == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    shim_blob_entry_t *entry = shim_find_blob(name);
    if (entry == NULL) {
        return SHIM_ERR_NOT_FOUND;
    }
    *out_offset = entry->offset;
    *out_len = entry->len;
    return SHIM_OK;
}
