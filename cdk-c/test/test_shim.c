#include <criterion/criterion.h>
#include <stdint.h>
#include <string.h>

#include "idl/cdk_alloc.h"
#include "shim/shim.h"

static const char    k_blob_name[] = "blob";
static const uint8_t k_blob_data[] = {1, 2, 3, 4, 5};

static int  g_log_calls = 0;
static char g_last_log[64];

static shim_result_t
mock_blob_size(void *ctx, const char *name, size_t *out_size) {
    (void)ctx;
    if (name == NULL || out_size == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    if (strcmp(name, k_blob_name) != 0) {
        return SHIM_ERR_NOT_FOUND;
    }
    *out_size = sizeof(k_blob_data);
    return SHIM_OK;
}

static shim_result_t mock_blob_read(
    void *ctx, const char *name, size_t offset, uint8_t *dst, size_t len) {
    (void)ctx;
    if (name == NULL || dst == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    if (strcmp(name, k_blob_name) != 0) {
        return SHIM_ERR_NOT_FOUND;
    }
    if (offset > sizeof(k_blob_data) || len > sizeof(k_blob_data) - offset) {
        return SHIM_ERR_OUT_OF_BOUNDS;
    }
    memcpy(dst, k_blob_data + offset, len);
    return SHIM_OK;
}

static shim_result_t
mock_map(void *ctx, const char *name, shim_map_t *out_map) {
    (void)ctx;
    if (name == NULL || out_map == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    if (strcmp(name, k_blob_name) != 0) {
        return SHIM_ERR_NOT_FOUND;
    }
    uint8_t *buffer = (uint8_t *)cdk_malloc(sizeof(k_blob_data));
    if (buffer == NULL) {
        return SHIM_ERR_OUT_OF_MEMORY;
    }
    memcpy(buffer, k_blob_data, sizeof(k_blob_data));
    out_map->data = buffer;
    out_map->len = sizeof(k_blob_data);
    out_map->kind = SHIM_MAP_OWNED;
    out_map->handle = buffer;
    return SHIM_OK;
}

static void mock_log(void *ctx, const char *msg) {
    (void)ctx;
    if (msg == NULL) {
        return;
    }
    g_log_calls += 1;
    strncpy(g_last_log, msg, sizeof(g_last_log) - 1);
    g_last_log[sizeof(g_last_log) - 1] = '\0';
}

static uint64_t mock_time_ns(void *ctx) {
    (void)ctx;
    return 123456789ULL;
}

static shim_result_t mock_getrandom(void *ctx, uint8_t *dst, size_t len) {
    (void)ctx;
    if (dst == NULL) {
        return SHIM_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < len; i++) {
        dst[i] = (uint8_t)(i & 0xFF);
    }
    return SHIM_OK;
}

static const shim_ops_t k_mock_ops = {
    .ctx = NULL,
    .blob_size = mock_blob_size,
    .blob_read = mock_blob_read,
    .map = mock_map,
    .unmap = NULL,
    .log = mock_log,
    .time_ns = mock_time_ns,
    .getrandom = mock_getrandom,
};

Test(shim, blob_size_and_read) {
    shim_set_ops(&k_mock_ops);

    size_t size = 0;
    cr_expect_eq(shim_blob_size(k_blob_name, &size), SHIM_OK);
    cr_expect_eq(size, sizeof(k_blob_data));

    uint8_t buffer[3] = {0};
    cr_expect_eq(shim_blob_read(k_blob_name, 1, buffer, sizeof(buffer)),
                 SHIM_OK);
    cr_expect_eq(buffer[0], 2);
    cr_expect_eq(buffer[1], 3);
    cr_expect_eq(buffer[2], 4);
}

Test(shim, map_unmap_resets_state) {
    shim_set_ops(&k_mock_ops);

    shim_map_t map = {0};
    cr_expect_eq(shim_map(k_blob_name, &map), SHIM_OK);
    cr_expect_eq(map.len, sizeof(k_blob_data));
    cr_expect_not_null(map.data);
    cr_expect_eq(map.kind, SHIM_MAP_OWNED);

    shim_unmap(&map);
    cr_expect_null(map.data);
    cr_expect_eq(map.len, 0);
    cr_expect_eq(map.kind, SHIM_MAP_BORROWED);
}

Test(shim, log_time_random_dispatch) {
    shim_set_ops(&k_mock_ops);

    g_log_calls = 0;
    g_last_log[0] = '\0';
    shim_log("hello");
    cr_expect_eq(g_log_calls, 1);
    cr_expect_str_eq(g_last_log, "hello");

    cr_expect_eq(shim_time_ns(), 123456789ULL);

    uint8_t buffer[4] = {0};
    cr_expect_eq(shim_getrandom(buffer, sizeof(buffer)), SHIM_OK);
    cr_expect_eq(buffer[0], 0);
    cr_expect_eq(buffer[1], 1);
    cr_expect_eq(buffer[2], 2);
    cr_expect_eq(buffer[3], 3);
}

Test(shim, blob_registry_roundtrip) {
    shim_clear_blobs();

    cr_expect_eq(shim_register_blob("asset", 123, 456), SHIM_OK);

    int64_t offset = 0;
    size_t  len = 0;
    cr_expect_eq(shim_lookup_blob("asset", &offset, &len), SHIM_OK);
    cr_expect_eq(offset, 123);
    cr_expect_eq(len, 456);

    cr_expect_eq(shim_unregister_blob("asset"), SHIM_OK);
    cr_expect_eq(shim_lookup_blob("asset", &offset, &len), SHIM_ERR_NOT_FOUND);
}
