// IC shim backend using ic_api + stable memory stubs.
#include "shim/shim.h"

#include "ic_api.h"
#include "ic_storage.h"
#include "idl/cdk_alloc.h"

#include <limits.h>
#include <stdint.h>

static shim_result_t
ic_blob_size(void *ctx, const char *name, size_t *out_size) {
    (void)ctx;
    if (out_size == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }

    int64_t       offset = 0;
    size_t        len = 0;
    shim_result_t res = shim_lookup_blob(name, &offset, &len);
    if (res != SHIM_OK) {
        return res;
    }
    *out_size = len;
    return SHIM_OK;
}

static shim_result_t ic_blob_read(
    void *ctx, const char *name, size_t offset, uint8_t *dst, size_t len) {
    (void)ctx;
    if (dst == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return SHIM_OK;
    }

    int64_t       base = 0;
    size_t        blob_len = 0;
    shim_result_t res = shim_lookup_blob(name, &base, &blob_len);
    if (res != SHIM_OK) {
        return res;
    }
    if (offset > blob_len || len > blob_len - offset) {
        return SHIM_ERR_OUT_OF_BOUNDS;
    }
    if (base > INT64_MAX - (int64_t)offset) {
        return SHIM_ERR_OUT_OF_BOUNDS;
    }

    ic_stable_read(dst, base + (int64_t)offset, (int64_t)len);
    return SHIM_OK;
}

static shim_result_t ic_map(void *ctx, const char *name, shim_map_t *out_map) {
    (void)ctx;
    if (out_map == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }

    int64_t       base = 0;
    size_t        blob_len = 0;
    shim_result_t res = shim_lookup_blob(name, &base, &blob_len);
    if (res != SHIM_OK) {
        return res;
    }

    uint8_t *buffer = NULL;
    if (blob_len > 0) {
        buffer = (uint8_t *)cdk_malloc(blob_len);
        if (buffer == NULL) {
            return SHIM_ERR_OUT_OF_MEMORY;
        }
        ic_stable_read(buffer, base, (int64_t)blob_len);
    }

    out_map->data = buffer;
    out_map->len = blob_len;
    out_map->kind = SHIM_MAP_OWNED;
    out_map->handle = buffer;
    return SHIM_OK;
}

static void ic_unmap(void *ctx, shim_map_t *map) {
    (void)ctx;
    if (map == NULL) {
        return;
    }
    if (map->kind == SHIM_MAP_OWNED && map->data != NULL) {
        cdk_free((void *)map->data);
    }
}

static void ic_log(void *ctx, const char *msg) {
    (void)ctx;
    if (msg == NULL) {
        return;
    }
    ic_api_debug_print(msg);
}

static uint64_t ic_time_ns(void *ctx) {
    (void)ctx;
    int64_t value = ic_api_time();
    if (value < 0) {
        return 0;
    }
    return (uint64_t)value;
}

static shim_result_t ic_getrandom(void *ctx, uint8_t *dst, size_t len) {
    (void)ctx;
    if (dst == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return SHIM_OK;
    }

    static uint64_t state = 0;
    if (state == 0) {
        state = ic_time_ns(NULL) ^ (uint64_t)(uintptr_t)&state;
    }

    for (size_t i = 0; i < len; i++) {
        state = state * 6364136223846793005ULL + 1;
        dst[i] = (uint8_t)(state >> 32);
    }
    return SHIM_OK;
}

static const shim_ops_t k_ic_ops = {
    .ctx = NULL,
    .blob_size = ic_blob_size,
    .blob_read = ic_blob_read,
    .map = ic_map,
    .unmap = ic_unmap,
    .log = ic_log,
    .time_ns = ic_time_ns,
    .getrandom = ic_getrandom,
};

const shim_ops_t *shim_ic_default_ops(void) { return &k_ic_ops; }
