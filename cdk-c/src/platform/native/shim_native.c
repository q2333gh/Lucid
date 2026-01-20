// Native shim backend using host libc/file system.
#include "shim/shim.h"

#include "idl/cdk_alloc.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static shim_result_t
native_blob_size(void *ctx, const char *name, size_t *out_size) {
    (void)ctx;
    if (name == NULL || out_size == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }

    struct stat st;
    if (stat(name, &st) != 0) {
        return SHIM_ERR_NOT_FOUND;
    }
    if (st.st_size < 0) {
        return SHIM_ERR_OUT_OF_BOUNDS;
    }
    if ((uint64_t)st.st_size > (uint64_t)SIZE_MAX) {
        return SHIM_ERR_OUT_OF_BOUNDS;
    }
    *out_size = (size_t)st.st_size;
    return SHIM_OK;
}

static shim_result_t native_blob_read(
    void *ctx, const char *name, size_t offset, uint8_t *dst, size_t len) {
    (void)ctx;
    if (name == NULL || dst == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return SHIM_OK;
    }

    size_t        size = 0;
    shim_result_t res = native_blob_size(NULL, name, &size);
    if (res != SHIM_OK) {
        return res;
    }
    if (offset > size || len > size - offset) {
        return SHIM_ERR_OUT_OF_BOUNDS;
    }

    FILE *fp = fopen(name, "rb");
    if (fp == NULL) {
        return SHIM_ERR_NOT_FOUND;
    }
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        fclose(fp);
        return SHIM_ERR_IO;
    }
    size_t read_len = fread(dst, 1, len, fp);
    fclose(fp);
    if (read_len != len) {
        return SHIM_ERR_IO;
    }
    return SHIM_OK;
}

static shim_result_t
native_map(void *ctx, const char *name, shim_map_t *out_map) {
    (void)ctx;
    if (name == NULL || out_map == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }

    size_t        size = 0;
    shim_result_t res = native_blob_size(NULL, name, &size);
    if (res != SHIM_OK) {
        return res;
    }

    uint8_t *buffer = NULL;
    if (size > 0) {
        buffer = (uint8_t *)cdk_malloc(size);
        if (buffer == NULL) {
            return SHIM_ERR_OUT_OF_MEMORY;
        }
        res = native_blob_read(NULL, name, 0, buffer, size);
        if (res != SHIM_OK) {
            cdk_free(buffer);
            return res;
        }
    }

    out_map->data = buffer;
    out_map->len = size;
    out_map->kind = SHIM_MAP_OWNED;
    out_map->handle = buffer;
    return SHIM_OK;
}

static void native_unmap(void *ctx, shim_map_t *map) {
    (void)ctx;
    if (map == NULL) {
        return;
    }
    if (map->kind == SHIM_MAP_OWNED && map->data != NULL) {
        cdk_free((void *)map->data);
    }
}

static void native_log(void *ctx, const char *msg) {
    (void)ctx;
    if (msg == NULL) {
        return;
    }
    fputs(msg, stderr);
}

static uint64_t native_time_ns(void *ctx) {
    (void)ctx;
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return (uint64_t)time(NULL) * 1000000000ULL;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static shim_result_t native_getrandom(void *ctx, uint8_t *dst, size_t len) {
    (void)ctx;
    if (dst == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return SHIM_OK;
    }

    static uint64_t state = 0;
    if (state == 0) {
        state = native_time_ns(NULL) ^ (uint64_t)(uintptr_t)&state;
    }

    for (size_t i = 0; i < len; i++) {
        state = state * 6364136223846793005ULL + 1;
        dst[i] = (uint8_t)(state >> 32);
    }
    return SHIM_OK;
}

static const shim_ops_t k_native_ops = {
    .ctx = NULL,
    .blob_size = native_blob_size,
    .blob_read = native_blob_read,
    .map = native_map,
    .unmap = native_unmap,
    .log = native_log,
    .time_ns = native_time_ns,
    .getrandom = native_getrandom,
};

const shim_ops_t *shim_native_default_ops(void) { return &k_native_ops; }
