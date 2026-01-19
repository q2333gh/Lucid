/*
 * IC Storage Implementation
 *
 * Implements storage interface using IC stable memory.
 */

#include "../../interface/storage.h"
#include "asset_manager.h"
#include "ic_c_sdk.h"
#include "ic_storage.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int
ic_read_checkpoint(void *ctx, const char *name, uint8_t **data, size_t *len) {
    ic_storage_ctx_t *ictx = (ic_storage_ctx_t *)ctx;
    if (!ictx || !name) {
        return -1;
    }

    asset_entry_t *entry = ic_asset_lookup(ictx, name);
    if (!entry || entry->length <= 0) {
        return -1;
    }

    if ((uint64_t)entry->length > SIZE_MAX) {
        return -1;
    }

    *len = (size_t)entry->length;
    *data = (uint8_t *)malloc(*len);
    if (!*data) {
        return -1;
    }

    ic_stable_read(*data, entry->offset, entry->length);
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
    ic_storage_ctx_t *ictx = (ic_storage_ctx_t *)ctx;
    if (!ictx || !name || !tokens || count <= 0 || offset < 0) {
        return -1;
    }

    asset_entry_t *entry = ic_asset_lookup(ictx, name);
    if (!entry || entry->length <= 0) {
        return -1;
    }

    int64_t byte_offset = entry->offset + (int64_t)offset * sizeof(uint16_t);
    int64_t byte_len = (int64_t)count * sizeof(uint16_t);

    if (byte_offset < entry->offset ||
        byte_offset + byte_len > entry->offset + entry->length) {
        return -1;
    }

    uint16_t *buffer = (uint16_t *)malloc((size_t)count * sizeof(uint16_t));
    if (!buffer) {
        return -1;
    }

    ic_stable_read((uint8_t *)buffer, byte_offset, byte_len);

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
