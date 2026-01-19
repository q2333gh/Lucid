#ifndef LLM_C_PLATFORM_IC_ASSET_MANAGER_H
#define LLM_C_PLATFORM_IC_ASSET_MANAGER_H

#include "ic_c_sdk.h"
#include "ic_storage.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_ASSETS 8
#define MAX_ASSET_NAME 64

typedef struct {
    char    name[MAX_ASSET_NAME];
    int64_t offset;
    int64_t length;
} asset_entry_t;

typedef struct {
    asset_entry_t       assets[MAX_ASSETS];
    size_t              asset_count;
    ic_stable_writer_t *writer;
} ic_storage_ctx_t;

ic_storage_ctx_t *ic_asset_manager_create(void);
void              ic_asset_manager_free(ic_storage_ctx_t *ctx);
asset_entry_t    *ic_asset_lookup(ic_storage_ctx_t *ctx, const char *name);
void              ic_asset_write(ic_storage_ctx_t *ctx,
                                 const char       *name,
                                 const uint8_t    *data,
                                 size_t            len);

#endif // LLM_C_PLATFORM_IC_ASSET_MANAGER_H
