/*
 * IC Storage Implementation
 *
 * Implements storage interface using IC stable memory.
 */

#include "../../interface/storage.h"
#include "asset_manager.h"
#include "ic_c_sdk.h"
#include "ic_storage.h"
#include "shim/shim.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int
ic_read_checkpoint(void *ctx, const char *name, uint8_t **data, size_t *len) {
    (void)ctx;
    if (!name || !data || !len) {
        return -1;
    }

    size_t size = 0;
    if (shim_blob_size(name, &size) != SHIM_OK) {
        return -1;
    }
    if (size > SIZE_MAX) {
        return -1;
    }
    *data = (uint8_t *)malloc(size);
    if (*data == NULL) {
        return -1;
    }
    if (shim_blob_read(name, 0, *data, size) != SHIM_OK) {
        free(*data);
        *data = NULL;
        return -1;
    }
    *len = size;
    return 0;
}

static int
ic_read_asset(void *ctx, const char *name, uint8_t **data, size_t *len) {
    return ic_read_checkpoint(ctx, name, data, len);
}

static int
ic_read_gguf(void *ctx, const char *name, uint8_t **data, size_t *len) {
    return ic_read_checkpoint(ctx, name, data, len);
}

static int ic_read_tokens(
    void *ctx, const char *name, int offset, int count, int **tokens) {
    (void)ctx;
    if (!name || !tokens || count <= 0 || offset < 0) {
        return -1;
    }

    size_t blob_len = 0;
    if (shim_blob_size(name, &blob_len) != SHIM_OK) {
        return -1;
    }

    int64_t byte_offset = (int64_t)offset * (int64_t)sizeof(uint16_t);
    int64_t byte_len = (int64_t)count * (int64_t)sizeof(uint16_t);
    if (byte_offset < 0 ||
        (uint64_t)byte_offset + (uint64_t)byte_len > blob_len) {
        return -1;
    }

    uint16_t *buffer = (uint16_t *)malloc((size_t)count * sizeof(uint16_t));
    if (!buffer) {
        return -1;
    }

    if (shim_blob_read(name, (size_t)byte_offset, (uint8_t *)buffer,
                       (size_t)byte_len) != SHIM_OK) {
        free(buffer);
        return -1;
    }

    *tokens = (int *)malloc((size_t)count * sizeof(int));
    if (!*tokens) {
        free(buffer);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        (*tokens)[i] = (int)buffer[i];
    }

    free(buffer);
    return 0;
}

llm_storage_t *llm_storage_create_ic(void) {
    ic_storage_ctx_t *ctx = ic_asset_manager_create();
    if (!ctx) {
        return NULL;
    }

    llm_storage_t *storage = (llm_storage_t *)malloc(sizeof(llm_storage_t));
    if (!storage) {
        ic_asset_manager_free(ctx);
        return NULL;
    }

    storage->read_checkpoint = ic_read_checkpoint;
    storage->read_asset = ic_read_asset;
    storage->read_tokens = ic_read_tokens;
    storage->read_gguf = ic_read_gguf;
    storage->context = ctx;

    return storage;
}

void llm_storage_free_ic(llm_storage_t *storage) {
    if (storage) {
        if (storage->context) {
            ic_asset_manager_free((ic_storage_ctx_t *)storage->context);
        }
        free(storage);
    }
}
