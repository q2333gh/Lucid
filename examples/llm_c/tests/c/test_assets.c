#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STABLE_TEST_PAGES 4

#include "../../src/canister/canister_helpers.h"
#include "ic_api.h"
#include "ic_storage.h"

// Emulate stable memory state using a fixed buffer.
static uint8_t g_test_stable[STABLE_TEST_PAGES * IC_STABLE_PAGE_SIZE_BYTES];

struct ic_stable_writer {
    int64_t offset;
    int64_t capacity;
};

static ic_stable_writer_t g_test_writer = {0, STABLE_TEST_PAGES};

int64_t ic_stable_size(void) { return g_test_writer.capacity; }

int64_t ic_stable_grow(int64_t new_pages) {
    g_test_writer.capacity += new_pages;
    return g_test_writer.capacity - new_pages;
}

ic_stable_writer_t *ic_stable_writer_create(void) {
    g_test_writer.offset = 0;
    g_test_writer.capacity = STABLE_TEST_PAGES;
    return &g_test_writer;
}

ic_stable_writer_t *ic_stable_writer_create_at(int64_t offset) {
    g_test_writer.offset = offset;
    return &g_test_writer;
}

ic_storage_result_t ic_stable_writer_write(ic_stable_writer_t *writer,
                                           const uint8_t      *data,
                                           size_t              len) {
    if (writer == NULL || data == NULL) {
        return IC_STORAGE_ERR_INVALID_ARG;
    }
    int64_t end = writer->offset + (int64_t)len;
    int64_t max_bytes =
        (int64_t)IC_STABLE_PAGE_SIZE_BYTES * g_test_writer.capacity;
    if (end > max_bytes) {
        return IC_STORAGE_ERR_OUT_OF_BOUNDS;
    }
    memcpy(g_test_stable + writer->offset, data, len);
    writer->offset = end;
    return IC_STORAGE_OK;
}

int64_t ic_stable_writer_offset(const ic_stable_writer_t *writer) {
    return writer ? writer->offset : 0;
}

void ic_stable_write(int64_t offset, const uint8_t *src, int64_t size) {
    if (offset < 0 || size < 0) {
        return;
    }
    memcpy(g_test_stable + offset, src, (size_t)size);
}

void ic_stable_read(uint8_t *dst, int64_t offset, int64_t size) {
    if (dst == NULL || offset < 0 || size < 0) {
        return;
    }
    memcpy(dst, g_test_stable + offset, (size_t)size);
}

void ic_api_debug_print(const char *msg) {
    if (msg != NULL) {
        fprintf(stderr, "%s", msg);
    }
}

void ic_api_debug_printf(const char *fmt, ...) {
    if (fmt == NULL) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

_Noreturn void ic_api_trap(const char *msg) {
    fprintf(stderr, "ic_api_trap: %s\n", msg ? msg : "null");
    exit(1);
}

static void expect(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "Test failure: %s\n", message);
        exit(1);
    }
}

static void
write_sample_asset(const char *name, const uint8_t *data, size_t len) {
    write_asset_data(name, data, len);
}

static void verify_asset_blob(const char    *name,
                              const uint8_t *expected,
                              size_t         expected_len) {
    uint8_t *blob = NULL;
    size_t   blob_len = 0;
    read_asset_blob(name, &blob, &blob_len);
    expect(blob_len == expected_len, "blob length mismatch");
    expect(memcmp(blob, expected, expected_len) == 0, "blob contents mismatch");
    free(blob);
}

static void verify_run_inference_stub(void) {
    char *reply = run_inference("ignored");
    expect(reply != NULL, "run_inference should return a stub message");
    expect(strcmp(reply, LLM_C_ASSETS_TESTING_STUB_REPLY) == 0,
           "run_inference stub reply mismatch");
    free(reply);
}

int main(void) {
    llm_c_reset_asset_state();
    expect(llm_c_asset_count() == 0, "asset table should start empty");

    static const uint8_t chunk_a[] = {0x10, 0x20};
    static const uint8_t chunk_b[] = {0x30, 0x40, 0x50};
    static const uint8_t chunk_expected[] = {0x10, 0x20, 0x30, 0x40, 0x50};

    write_sample_asset("chunked", chunk_a, sizeof chunk_a);
    write_sample_asset("chunked", chunk_b, sizeof chunk_b);
    expect(llm_c_asset_count() == 1, "expected chunked asset stored");
    verify_asset_blob("chunked", chunk_expected, sizeof chunk_expected);

    static const uint8_t checkpoint_data[] = {0x11, 0x22, 0x33, 0x44};
    static const uint8_t vocab_data[] = {0xAA, 0xBB, 0xCC};
    static const uint8_t merges_data[] = {0xDE, 0xAD, 0xBE, 0xEF};

    write_sample_asset("checkpoint", checkpoint_data, sizeof checkpoint_data);
    write_sample_asset("vocab", vocab_data, sizeof vocab_data);
    write_sample_asset("merges", merges_data, sizeof merges_data);

    expect(llm_c_asset_count() == 4, "expected four assets stored");

    require_assets_loaded();

    verify_asset_blob("checkpoint", checkpoint_data, sizeof checkpoint_data);
    verify_asset_blob("vocab", vocab_data, sizeof vocab_data);
    verify_asset_blob("merges", merges_data, sizeof merges_data);

    verify_run_inference_stub();

    puts("llm_c asset helpers test passed");
    return 0;
}
