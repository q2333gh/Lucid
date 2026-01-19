/**
 * Subset of prim.test.did blob cases for c_candid decoding.
 * Focuses on binary inputs and basic success/failure assertions.
 */
#include "idl/runtime.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct test_case {
    const char *name;
    const char *blob;
    idl_type *(*type_builder)(idl_arena *arena);
    bool           expect_ok;
    idl_value_kind expected_kind;
    bool           allow_nan;
    int            inf_sign;
    union {
        uint8_t     nat8_val;
        uint16_t    nat16_val;
        uint32_t    nat32_val;
        uint64_t    nat64_val;
        int8_t      int8_val;
        int16_t     int16_val;
        int32_t     int32_val;
        int64_t     int64_val;
        float       float32_val;
        double      float64_val;
        const char *text_val;
        int         bool_val;
    } expected;
} test_case;

static idl_type *type_bool(idl_arena *arena) { return idl_type_bool(arena); }
static idl_type *type_null(idl_arena *arena) { return idl_type_null(arena); }
static idl_type *type_reserved(idl_arena *arena) {
    return idl_type_reserved(arena);
}
static idl_type *type_empty(idl_arena *arena) { return idl_type_empty(arena); }
static idl_type *type_nat(idl_arena *arena) { return idl_type_nat(arena); }
static idl_type *type_int(idl_arena *arena) { return idl_type_int(arena); }
static idl_type *type_nat8(idl_arena *arena) { return idl_type_nat8(arena); }
static idl_type *type_nat16(idl_arena *arena) { return idl_type_nat16(arena); }
static idl_type *type_nat32(idl_arena *arena) { return idl_type_nat32(arena); }
static idl_type *type_nat64(idl_arena *arena) { return idl_type_nat64(arena); }
static idl_type *type_int8(idl_arena *arena) { return idl_type_int8(arena); }
static idl_type *type_int16(idl_arena *arena) { return idl_type_int16(arena); }
static idl_type *type_int32(idl_arena *arena) { return idl_type_int32(arena); }
static idl_type *type_int64(idl_arena *arena) { return idl_type_int64(arena); }
static idl_type *type_float32(idl_arena *arena) {
    return idl_type_float32(arena);
}
static idl_type *type_float64(idl_arena *arena) {
    return idl_type_float64(arena);
}
static idl_type *type_text(idl_arena *arena) { return idl_type_text(arena); }

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static uint8_t *parse_blob_string(const char *input, size_t *out_len) {
    size_t   len = strlen(input);
    uint8_t *buf = malloc(len);
    if (!buf) {
        return NULL;
    }

    size_t i = 0;
    size_t w = 0;
    while (i < len) {
        if (input[i] == '\\') {
            if (i + 2 >= len) {
                free(buf);
                return NULL;
            }
            int hi = hex_nibble(input[i + 1]);
            int lo = hex_nibble(input[i + 2]);
            if (hi < 0 || lo < 0) {
                free(buf);
                return NULL;
            }
            buf[w++] = (uint8_t)((hi << 4) | lo);
            i += 3;
            continue;
        }
        buf[w++] = (uint8_t)input[i];
        i++;
    }

    *out_len = w;
    return buf;
}

static bool value_matches(const test_case *tc, const idl_value *value) {
    if (!value || value->kind != tc->expected_kind) {
        return false;
    }

    switch (tc->expected_kind) {
    case IDL_VALUE_NULL:
    case IDL_VALUE_RESERVED:
        return true;
    case IDL_VALUE_NAT:
    case IDL_VALUE_INT:
        return true;
    case IDL_VALUE_BOOL:
        return value->data.bool_val == tc->expected.bool_val;
    case IDL_VALUE_NAT8:
        return value->data.nat8_val == tc->expected.nat8_val;
    case IDL_VALUE_NAT16:
        return value->data.nat16_val == tc->expected.nat16_val;
    case IDL_VALUE_NAT32:
        return value->data.nat32_val == tc->expected.nat32_val;
    case IDL_VALUE_NAT64:
        return value->data.nat64_val == tc->expected.nat64_val;
    case IDL_VALUE_INT8:
        return value->data.int8_val == tc->expected.int8_val;
    case IDL_VALUE_INT16:
        return value->data.int16_val == tc->expected.int16_val;
    case IDL_VALUE_INT32:
        return value->data.int32_val == tc->expected.int32_val;
    case IDL_VALUE_INT64:
        return value->data.int64_val == tc->expected.int64_val;
    case IDL_VALUE_FLOAT32:
        if (tc->allow_nan) {
            return value->data.float32_val != value->data.float32_val;
        }
        if (tc->inf_sign != 0) {
            return isinf(value->data.float32_val) &&
                   (signbit(value->data.float32_val) ? -1 : 1) == tc->inf_sign;
        }
        return (value->data.float32_val - tc->expected.float32_val) < 0.0001f &&
               (tc->expected.float32_val - value->data.float32_val) < 0.0001f;
    case IDL_VALUE_FLOAT64:
        if (tc->allow_nan) {
            return value->data.float64_val != value->data.float64_val;
        }
        if (tc->inf_sign != 0) {
            return isinf(value->data.float64_val) &&
                   (signbit(value->data.float64_val) ? -1 : 1) == tc->inf_sign;
        }
        return (value->data.float64_val - tc->expected.float64_val) <
                   0.000001 &&
               (tc->expected.float64_val - value->data.float64_val) < 0.000001;
    case IDL_VALUE_TEXT:
        if (!value->data.text.data || !tc->expected.text_val) {
            return false;
        }
        if (value->data.text.len != strlen(tc->expected.text_val)) {
            return false;
        }
        return memcmp(value->data.text.data, tc->expected.text_val,
                      value->data.text.len) == 0;
    default:
        return false;
    }
}

#define CASE_STD(name, blob, type, ok, kind, field)                            \
    { name, blob, type, ok, kind, false, 0, field }
#define CASE_NAN(name, blob, type, kind)                                       \
    {                                                                          \
        name, blob, type, true, kind, true, 0, { .float64_val = 0.0 }          \
    }
#define CASE_INF(name, blob, type, kind, sign)                                 \
    {                                                                          \
        name, blob, type, true, kind, false, sign, { .float64_val = 0.0 }      \
    }

int main(void) {
    const test_case cases[] = {
        CASE_STD("null", "DIDL\\00\\01\\7f", type_null, true, IDL_VALUE_NULL,
                 {.bool_val = 0}),
        CASE_STD("reserved", "DIDL\\00\\01\\70", type_reserved, true,
                 IDL_VALUE_RESERVED, {.bool_val = 0}),
        CASE_STD("empty rejects", "DIDL\\00\\01\\6f", type_empty, false,
                 IDL_VALUE_NULL, {.bool_val = 0}),
        CASE_STD("bool false", "DIDL\\00\\01\\7e\\00", type_bool, true,
                 IDL_VALUE_BOOL, {.bool_val = 0}),
        CASE_STD("bool true", "DIDL\\00\\01\\7e\\01", type_bool, true,
                 IDL_VALUE_BOOL, {.bool_val = 1}),
        CASE_STD("bool out of range", "DIDL\\00\\01\\7e\\02", type_bool, false,
                 IDL_VALUE_BOOL, {.bool_val = 0}),
        CASE_STD("nat leb128 300", "DIDL\\00\\01\\7d\\ac\\02", type_nat, true,
                 IDL_VALUE_NAT, {.bool_val = 0}),
        CASE_STD("nat leb128 truncated", "DIDL\\00\\01\\7d\\80", type_nat,
                 false, IDL_VALUE_NAT, {.bool_val = 0}),
        CASE_STD("int sleb128 -1", "DIDL\\00\\01\\7c\\7f", type_int, true,
                 IDL_VALUE_INT, {.bool_val = 0}),
        CASE_STD("int sleb128 truncated", "DIDL\\00\\01\\7c\\80", type_int,
                 false, IDL_VALUE_INT, {.bool_val = 0}),
        CASE_STD("nat8 max", "DIDL\\00\\01\\7b\\ff", type_nat8, true,
                 IDL_VALUE_NAT8, {.nat8_val = 255}),
        CASE_STD("nat8 too long", "DIDL\\00\\01\\7b\\ff\\00", type_nat8, false,
                 IDL_VALUE_NAT8, {.nat8_val = 0}),
        CASE_STD("nat16 256", "DIDL\\00\\01\\7a\\00\\01", type_nat16, true,
                 IDL_VALUE_NAT16, {.nat16_val = 256}),
        CASE_STD("nat16 too short", "DIDL\\00\\01\\7a\\00", type_nat16, false,
                 IDL_VALUE_NAT16, {.nat16_val = 0}),
        CASE_STD("nat32 too long", "DIDL\\00\\01\\79\\00\\00\\00\\00\\00",
                 type_nat32, false, IDL_VALUE_NAT32, {.nat32_val = 0}),
        CASE_STD("nat32 max", "DIDL\\00\\01\\79\\ff\\ff\\ff\\ff", type_nat32,
                 true, IDL_VALUE_NAT32, {.nat32_val = 4294967295u}),
        CASE_STD("nat64 too short", "DIDL\\00\\01\\78\\00", type_nat64, false,
                 IDL_VALUE_NAT64, {.nat64_val = 0}),
        CASE_STD("nat64 max",
                 "DIDL\\00\\01\\78\\ff\\ff\\ff\\ff\\ff\\ff\\ff\\ff", type_nat64,
                 true, IDL_VALUE_NAT64, {.nat64_val = 18446744073709551615ull}),
        CASE_STD("int8 -1", "DIDL\\00\\01\\77\\ff", type_int8, true,
                 IDL_VALUE_INT8, {.int8_val = -1}),
        CASE_STD("int8 too long", "DIDL\\00\\01\\77\\00\\00", type_int8, false,
                 IDL_VALUE_INT8, {.int8_val = 0}),
        CASE_STD("int16 -2", "DIDL\\00\\01\\76\\fe\\ff", type_int16, true,
                 IDL_VALUE_INT16, {.int16_val = -2}),
        CASE_STD("int16 too short", "DIDL\\00\\01\\76\\00", type_int16, false,
                 IDL_VALUE_INT16, {.int16_val = 0}),
        CASE_STD("int32 -1", "DIDL\\00\\01\\75\\ff\\ff\\ff\\ff", type_int32,
                 true, IDL_VALUE_INT32, {.int32_val = -1}),
        CASE_STD("int32 too short", "DIDL\\00\\01\\75\\00\\00\\00", type_int32,
                 false, IDL_VALUE_INT32, {.int32_val = 0}),
        CASE_STD("int64 256",
                 "DIDL\\00\\01\\74\\00\\01\\00\\00\\00\\00\\00\\00", type_int64,
                 true, IDL_VALUE_INT64, {.int64_val = 256}),
        CASE_STD("int64 too long",
                 "DIDL\\00\\01\\74\\00\\00\\00\\00\\00\\00\\00\\00\\00",
                 type_int64, false, IDL_VALUE_INT64, {.int64_val = 0}),
        CASE_STD("float32 0.5", "DIDL\\00\\01\\73\\00\\00\\00\\3f",
                 type_float32, true, IDL_VALUE_FLOAT32, {.float32_val = 0.5f}),
        CASE_NAN("float32 NaN", "DIDL\\00\\01\\73\\00\\00\\c0\\7f",
                 type_float32, IDL_VALUE_FLOAT32),
        CASE_INF("float32 +inf", "DIDL\\00\\01\\73\\00\\00\\80\\7f",
                 type_float32, IDL_VALUE_FLOAT32, 1),
        CASE_INF("float32 -inf", "DIDL\\00\\01\\73\\00\\00\\80\\ff",
                 type_float32, IDL_VALUE_FLOAT32, -1),
        CASE_STD("float32 too long", "DIDL\\00\\01\\73\\00\\00\\00\\00\\00",
                 type_float32, false, IDL_VALUE_FLOAT32, {.float32_val = 0.0f}),
        CASE_STD("float64 -0.5",
                 "DIDL\\00\\01\\72\\00\\00\\00\\00\\00\\00\\e0\\bf",
                 type_float64, true, IDL_VALUE_FLOAT64, {.float64_val = -0.5}),
        CASE_NAN("float64 NaN",
                 "DIDL\\00\\01\\72\\00\\00\\00\\00\\00\\00\\f8\\7f",
                 type_float64, IDL_VALUE_FLOAT64),
        CASE_INF("float64 +inf",
                 "DIDL\\00\\01\\72\\00\\00\\00\\00\\00\\00\\f0\\7f",
                 type_float64, IDL_VALUE_FLOAT64, 1),
        CASE_INF("float64 -inf",
                 "DIDL\\00\\01\\72\\00\\00\\00\\00\\00\\00\\f0\\ff",
                 type_float64, IDL_VALUE_FLOAT64, -1),
        CASE_STD("float64 too short", "DIDL\\00\\01\\72\\00\\00\\00\\00",
                 type_float64, false, IDL_VALUE_FLOAT64, {.float64_val = 0.0}),
        CASE_STD("text Motoko", "DIDL\\00\\01\\71\\06Motoko", type_text, true,
                 IDL_VALUE_TEXT, {.text_val = "Motoko"}),
        CASE_STD("text too short", "DIDL\\00\\01\\71\\07Motoko", type_text,
                 false, IDL_VALUE_TEXT, {.text_val = "Motoko"}),
        CASE_STD("text invalid utf8", "DIDL\\00\\01\\71\\03\\e2\\28\\a1",
                 type_text, false, IDL_VALUE_TEXT, {.text_val = ""}),
        CASE_STD("text unicode overshoot", "DIDL\\00\\01\\71\\02\\e2\\98\\83",
                 type_text, false, IDL_VALUE_TEXT, {.text_val = ""}),
        CASE_STD("wrong magic", "DADL", type_bool, false, IDL_VALUE_BOOL,
                 {.bool_val = 0}),
    };

    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const test_case *tc = &cases[i];
        size_t           len = 0;
        uint8_t         *data = parse_blob_string(tc->blob, &len);
        if (!data) {
            printf("FAIL: %s (parse error)\n", tc->name);
            failures++;
            continue;
        }

        idl_arena arena;
        if (idl_arena_init(&arena, 4096) != IDL_STATUS_OK) {
            printf("FAIL: %s (arena init)\n", tc->name);
            free(data);
            failures++;
            continue;
        }

        idl_deserializer *de = NULL;
        idl_status        st = idl_deserializer_new(data, len, &arena, &de);
        if (st != IDL_STATUS_OK) {
            if (tc->expect_ok) {
                printf("FAIL: %s (deserializer init)\n", tc->name);
                failures++;
            }
            idl_arena_destroy(&arena);
            free(data);
            continue;
        }

        idl_value *decoded = NULL;
        st = idl_deserializer_get_value_with_type(de, tc->type_builder(&arena),
                                                  &decoded);
        idl_status done_st = IDL_STATUS_OK;
        if (st == IDL_STATUS_OK) {
            done_st = idl_deserializer_done(de);
        }

        if (tc->expect_ok) {
            if (st != IDL_STATUS_OK || done_st != IDL_STATUS_OK ||
                !value_matches(tc, decoded)) {
                printf("FAIL: %s (decode mismatch)\n", tc->name);
                failures++;
            }
        } else {
            if (st == IDL_STATUS_OK && done_st == IDL_STATUS_OK) {
                printf("FAIL: %s (expected failure)\n", tc->name);
                failures++;
            }
        }

        idl_arena_destroy(&arena);
        free(data);
    }

    if (failures == 0) {
        printf("PASS: prim subset tests\n");
        return 0;
    }
    printf("FAIL: prim subset tests (%d failures)\n", failures);
    return 1;
}
