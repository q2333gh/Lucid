/**
 * subset of construct.test.did focusing on record decoding.
 */
#include "idl/runtime.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef idl_type *(*record_type_builder)(idl_arena *);

typedef struct record_case {
    const char         *name;
    const char         *blob;
    record_type_builder builder;
    bool                expect_ok;
} record_case;

typedef struct record_val_case {
    const char    *name;
    const char    *blob;
    size_t         expect_len;
    idl_value_kind expect_kind0;
    idl_value_kind expect_kind1;
} record_val_case;

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static uint8_t *parse_blob(const char *input, size_t *out_len) {
    size_t   len = strlen(input);
    uint8_t *buf = malloc(len);
    if (!buf)
        return NULL;
    size_t i = 0, w = 0;
    while (i < len) {
        if (input[i] == '\\') {
            if (i + 3 >= len) {
                free(buf);
                fprintf(stderr, "parse: truncated escape at %zu\n", i);
                return NULL;
            }
            if (input[i + 1] != 'x') {
                free(buf);
                fprintf(stderr, "parse: unsupported escape at %zu\n", i);
                return NULL;
            }
            int hi = hex_nibble(input[i + 2]);
            int lo = hex_nibble(input[i + 3]);
            if (hi < 0 || lo < 0) {
                free(buf);
                fprintf(stderr, "parse: invalid escape at %zu\n", i);
                return NULL;
            }
            buf[w++] = (uint8_t)((hi << 4) | lo);
            i += 4;
            continue;
        }
        buf[w++] = (uint8_t)input[i++];
    }
    *out_len = w;
    return buf;
}

static idl_type *build_empty_record(idl_arena *arena) {
    return idl_type_record(arena, NULL, 0);
}

static idl_type *build_single_int_field(idl_arena *arena) {
    idl_field *fields = idl_arena_alloc(arena, sizeof(idl_field));
    if (!fields) {
        return NULL;
    }
    fields[0].label.kind = IDL_LABEL_ID;
    fields[0].label.id = 1;
    fields[0].type = idl_type_int32(arena);
    if (!fields[0].type) {
        return NULL;
    }
    return idl_type_record(arena, fields, 1);
}

static bool check_record_values(const idl_value       *value,
                                const record_val_case *expect) {
    if (!value || value->kind != IDL_VALUE_RECORD) {
        return false;
    }
    if (value->data.record.len != expect->expect_len) {
        return false;
    }
    int seen0 = 0;
    int seen1 = 0;
    for (size_t i = 0; i < value->data.record.len; ++i) {
        idl_value *v = value->data.record.fields[i].value;
        if (!v) {
            continue;
        }
        if (v->kind == expect->expect_kind0) {
            seen0++;
        }
        if (v->kind == expect->expect_kind1) {
            seen1++;
        }
    }
    return seen0 > 0 && seen1 > 0;
}

int main(void) {
    const record_case cases[] = {
        {"empty record",         "DIDL\\x01\\x6c\\x00\\x01\\x00",                build_empty_record,
         true                                                                                             },
        {"record missing field", "DIDL\\x01\\x6c\\x01\\x01\\x7c\\x02\\x00\\x2a",
         build_single_int_field,                                                                     false},
    };

    const record_val_case val_cases[] = {
        {"named fields",
         "DIDL\\x01\\x6c\\x02\\x00\\x7c\\x01\\x7e\\x01\\x00\\x2a\\x01", 2,
         IDL_VALUE_INT, IDL_VALUE_BOOL},
    };

    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        size_t   len = 0;
        uint8_t *data = parse_blob(cases[i].blob, &len);
        if (!data) {
            printf("FAIL: %s (parse)\n", cases[i].name);
            failures++;
            continue;
        }
        idl_arena arena;
        if (idl_arena_init(&arena, 4096) != IDL_STATUS_OK) {
            printf("FAIL: %s (arena)\n", cases[i].name);
            free(data);
            failures++;
            continue;
        }
        idl_deserializer *de = NULL;
        idl_status        st = idl_deserializer_new(data, len, &arena, &de);
        idl_type         *wire_type = NULL;
        idl_value        *wire_value = NULL;
        if (st == IDL_STATUS_OK) {
            st = idl_deserializer_get_value(de, &wire_type, &wire_value);
        }
        idl_status done = (st == IDL_STATUS_OK) ? idl_deserializer_done(de)
                                                : IDL_STATUS_ERR_INVALID_ARG;
        bool       ok = st == IDL_STATUS_OK && done == IDL_STATUS_OK;
        if (ok && cases[i].builder) {
            idl_type *expected = cases[i].builder(&arena);
            if (expected) {
                idl_value *coerced = NULL;
                idl_status coerce_st =
                    idl_coerce_value(&arena, &de->header.env, wire_type,
                                     expected, wire_value, &coerced);
                ok = ok && coerce_st == IDL_STATUS_OK;
            }
        }
        if (ok != cases[i].expect_ok) {
            printf("DEBUG: %s st=%d done=%d expect=%d\n", cases[i].name, st,
                   done, cases[i].expect_ok);
            printf("FAIL: %s (expectMismatch)\n", cases[i].name);
            failures++;
        }
        idl_arena_destroy(&arena);
        free(data);
    }

    for (size_t i = 0; i < sizeof(val_cases) / sizeof(val_cases[0]); ++i) {
        size_t    len = 0;
        uint8_t  *data = parse_blob(val_cases[i].blob, &len);
        idl_arena arena;
        idl_arena_init(&arena, 4096);
        idl_deserializer *de;
        idl_type         *wire = NULL;
        idl_value        *decoded = NULL;
        idl_status        st = idl_deserializer_new(data, len, &arena, &de);
        if (st == IDL_STATUS_OK) {
            st = idl_deserializer_get_value(de, &wire, &decoded);
        }
        idl_status done = (st == IDL_STATUS_OK) ? idl_deserializer_done(de)
                                                : IDL_STATUS_ERR_INVALID_ARG;
        if (st != IDL_STATUS_OK || done != IDL_STATUS_OK ||
            !check_record_values(decoded, &val_cases[i])) {
            printf("FAIL: %s (value)\n", val_cases[i].name);
            failures++;
        }
        idl_arena_destroy(&arena);
        free(data);
    }

    if (failures == 0) {
        printf("PASS: record subset\n");
        return 0;
    }
    printf("FAIL: record subset (%d)\n", failures);
    return 1;
}
