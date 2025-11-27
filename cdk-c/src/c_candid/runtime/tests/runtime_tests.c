#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "idl/runtime.h"

static void test_uleb128_roundtrip(uint64_t value) {
    uint8_t buf[16];
    size_t written = 0;
    uint64_t decoded = 0;
    size_t consumed = 0;
    assert(idl_uleb128_encode(value, buf, sizeof(buf), &written) == IDL_STATUS_OK);
    assert(idl_uleb128_decode(buf, written, &consumed, &decoded) == IDL_STATUS_OK);
    assert(consumed == written);
    assert(decoded == value);
}

static void test_sleb128_roundtrip(int64_t value) {
    uint8_t buf[16];
    size_t written = 0;
    int64_t decoded = 0;
    size_t consumed = 0;
    assert(idl_sleb128_encode(value, buf, sizeof(buf), &written) == IDL_STATUS_OK);
    assert(idl_sleb128_decode(buf, written, &consumed, &decoded) == IDL_STATUS_OK);
    assert(consumed == written);
    assert(decoded == value);
}

static void test_uleb128_overflow(void) {
    uint8_t buf[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02};
    uint64_t out = 0;
    size_t consumed = 0;
    assert(idl_uleb128_decode(buf, sizeof(buf), &consumed, &out) == IDL_STATUS_ERR_OVERFLOW);
}

static void test_hash(void) {
    assert(idl_hash("name") == idl_hash("name"));
    assert(idl_hash("name") != idl_hash("value"));
    idl_field_id fields[] = {
        {.id = 5, .index = 1},
        {.id = 2, .index = 0},
        {.id = 30, .index = 2},
    };
    idl_field_id_sort(fields, 3);
    assert(fields[0].id == 2);
    assert(fields[1].id == 5);
    assert(fields[2].id == 30);
    assert(idl_field_id_verify_unique(fields, 3) == IDL_STATUS_OK);
    fields[1].id = 2;
    assert(idl_field_id_verify_unique(fields, 3) == IDL_STATUS_ERR_INVALID_ARG);
}

static void test_arena(void) {
    idl_arena arena;
    assert(idl_arena_init(&arena, 128) == IDL_STATUS_OK);
    char *hello = (char *)idl_arena_alloc(&arena, 6);
    assert(hello);
    memcpy(hello, "hello", 6);
    char *copy = (char *)idl_arena_dup(&arena, "world", 6);
    assert(copy);
    assert(memcmp(copy, "world", 6) == 0);
    int *numbers = (int *)idl_arena_alloc_zeroed(&arena, 4 * sizeof(int));
    assert(numbers);
    for (int i = 0; i < 4; ++i) {
        assert(numbers[i] == 0);
    }
    idl_arena_destroy(&arena);
}

int main(void) {
    test_uleb128_roundtrip(0);
    test_uleb128_roundtrip(1);
    test_uleb128_roundtrip(127);
    test_uleb128_roundtrip(128);
    test_uleb128_roundtrip(UINT64_C(0xffffffffffffffff));

    test_sleb128_roundtrip(0);
    test_sleb128_roundtrip(-1);
    test_sleb128_roundtrip(63);
    test_sleb128_roundtrip(-64);
    test_sleb128_roundtrip(INT64_C(0x7fffffffffffffff));
    test_sleb128_roundtrip(INT64_C(-0x7fffffffffffffff - 1));

    test_uleb128_overflow();
    test_hash();
    test_arena();

    puts("runtime tests passed");
    return 0;
}


