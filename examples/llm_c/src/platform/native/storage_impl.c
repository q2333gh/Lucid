/*
 * Native Storage Implementation
 *
 * Implements storage interface using native file system.
 */

#include "../../interface/storage.h"
#include "shim/shim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *checkpoint_path;
    const char *vocab_path;
    const char *merges_path;
    const char *gguf_path; // GGUF model file path
} native_storage_ctx_t;

static int native_read_checkpoint(void       *ctx,
                                  const char *name,
                                  uint8_t   **data,
                                  size_t     *len) {
    native_storage_ctx_t *nctx = (native_storage_ctx_t *)ctx;
    if (!nctx || !nctx->checkpoint_path || !data || !len) {
        return -1;
    }

    size_t size = 0;
    if (shim_blob_size(nctx->checkpoint_path, &size) != SHIM_OK) {
        return -1;
    }
    *data = (uint8_t *)malloc(size);
    if (*data == NULL) {
        return -1;
    }
    if (shim_blob_read(nctx->checkpoint_path, 0, *data, size) != SHIM_OK) {
        free(*data);
        *data = NULL;
        return -1;
    }
    *len = size;

    return 0;
}

static int
native_read_asset(void *ctx, const char *name, uint8_t **data, size_t *len) {
    native_storage_ctx_t *nctx = (native_storage_ctx_t *)ctx;
    if (!nctx || !data || !len) {
        return -1;
    }

    const char *path = NULL;
    if (strcmp(name, "vocab") == 0) {
        path = nctx->vocab_path;
    } else if (strcmp(name, "merges") == 0) {
        path = nctx->merges_path;
    } else {
        return -1;
    }

    if (!path) {
        return -1;
    }

    size_t size = 0;
    if (shim_blob_size(path, &size) != SHIM_OK) {
        return -1;
    }
    *data = (uint8_t *)malloc(size);
    if (*data == NULL) {
        return -1;
    }
    if (shim_blob_read(path, 0, *data, size) != SHIM_OK) {
        free(*data);
        *data = NULL;
        return -1;
    }
    *len = size;

    return 0;
}

static int native_read_tokens(
    void *ctx, const char *name, int offset, int count, int **tokens) {
    // Not implemented for native platform
    (void)ctx;
    (void)name;
    (void)offset;
    (void)count;
    (void)tokens;
    return -1;
}

static int
native_read_gguf(void *ctx, const char *name, uint8_t **data, size_t *len) {
    native_storage_ctx_t *nctx = (native_storage_ctx_t *)ctx;
    if (!nctx || !data || !len) {
        return -1;
    }

    const char *path = nctx->gguf_path ? nctx->gguf_path : name;

    if (!path) {
        return -1;
    }

    size_t size = 0;
    if (shim_blob_size(path, &size) != SHIM_OK) {
        return -1;
    }
    *data = (uint8_t *)malloc(size);
    if (*data == NULL) {
        return -1;
    }
    if (shim_blob_read(path, 0, *data, size) != SHIM_OK) {
        free(*data);
        *data = NULL;
        return -1;
    }
    *len = size;

    return 0;
}

llm_storage_t *llm_storage_create_native_with_gguf(const char *checkpoint_path,
                                                   const char *vocab_path,
                                                   const char *merges_path,
                                                   const char *gguf_path);

llm_storage_t *llm_storage_create_native(const char *checkpoint_path,
                                         const char *vocab_path,
                                         const char *merges_path) {
    return llm_storage_create_native_with_gguf(checkpoint_path, vocab_path,
                                               merges_path, NULL);
}

llm_storage_t *llm_storage_create_native_with_gguf(const char *checkpoint_path,
                                                   const char *vocab_path,
                                                   const char *merges_path,
                                                   const char *gguf_path) {
    native_storage_ctx_t *ctx =
        (native_storage_ctx_t *)malloc(sizeof(native_storage_ctx_t));
    if (!ctx) {
        return NULL;
    }

    ctx->checkpoint_path = checkpoint_path;
    ctx->vocab_path = vocab_path;
    ctx->merges_path = merges_path;
    ctx->gguf_path = gguf_path;

    llm_storage_t *storage = (llm_storage_t *)malloc(sizeof(llm_storage_t));
    if (!storage) {
        free(ctx);
        return NULL;
    }

    storage->read_checkpoint = native_read_checkpoint;
    storage->read_asset = native_read_asset;
    storage->read_tokens = native_read_tokens;
    storage->read_gguf = native_read_gguf;
    storage->context = ctx;

    return storage;
}

void llm_storage_free_native(llm_storage_t *storage) {
    if (storage) {
        if (storage->context) {
            free(storage->context);
        }
        free(storage);
    }
}
