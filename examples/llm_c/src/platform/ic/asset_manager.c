/*
 * IC Asset Manager
 *
 * Manages assets in IC stable memory.
 */

#include "asset_manager.h"
#include "ic_c_sdk.h"
#include "ic_storage.h"
#include "shim/shim.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

ic_storage_ctx_t *ic_asset_manager_create(void) {
    ic_storage_ctx_t *ctx =
        (ic_storage_ctx_t *)malloc(sizeof(ic_storage_ctx_t));
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(ic_storage_ctx_t));
    ctx->writer = ic_stable_writer_create();
    if (!ctx->writer) {
        free(ctx);
        return NULL;
    }

    shim_clear_blobs();
    return ctx;
}

void ic_asset_manager_free(ic_storage_ctx_t *ctx) {
    if (ctx) {
        // Note: ic_stable_writer doesn't need explicit cleanup
        // The writer is automatically managed by the IC runtime
        for (size_t i = 0; i < ctx->asset_count; i++) {
            shim_unregister_blob(ctx->assets[i].name);
        }
        free(ctx);
    }
}

asset_entry_t *ic_asset_lookup(ic_storage_ctx_t *ctx, const char *name) {
    if (!ctx || !name) {
        return NULL;
    }

    for (size_t i = 0; i < ctx->asset_count; i++) {
        if (strncmp(ctx->assets[i].name, name, MAX_ASSET_NAME) == 0) {
            return &ctx->assets[i];
        }
    }

    return NULL;
}

void ic_asset_write(ic_storage_ctx_t *ctx,
                    const char       *name,
                    const uint8_t    *data,
                    size_t            len) {
    if (!ctx || !name || !data || len == 0) {
        return;
    }

    // Note: len is size_t (unsigned), comparison with INT64_MAX removed
    // as it's always false on 64-bit platforms

    asset_entry_t *entry = ic_asset_lookup(ctx, name);
    if (!entry) {
        if (ctx->asset_count >= MAX_ASSETS) {
            return;
        }
        entry = &ctx->assets[ctx->asset_count++];
        memset(entry, 0, sizeof(*entry));
        size_t copy_len = strlen(name);
        if (copy_len >= MAX_ASSET_NAME) {
            copy_len = MAX_ASSET_NAME - 1;
        }
        memcpy(entry->name, name, copy_len);
        entry->name[copy_len] = '\0';
        entry->offset = ic_stable_writer_offset(ctx->writer);
        entry->length = 0;
    }

    int64_t start = ic_stable_writer_offset(ctx->writer);
    if (start != entry->offset + entry->length) {
        return;
    }

    ic_storage_result_t result =
        ic_stable_writer_write(ctx->writer, data, (int64_t)len);
    if (result != IC_STORAGE_OK) {
        return;
    }

    entry->length += (int64_t)len;
    shim_register_blob(entry->name, entry->offset, (size_t)entry->length);
}
