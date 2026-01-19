/**
 * Subset of reference.test.did blob cases for c_candid decoding.
 * Focuses on principal decoding success/failure.
 */
#include "idl/runtime.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct principal_case {
    const char    *name;
    const char    *blob;
    bool           expect_ok;
    const uint8_t *expected_bytes;
    size_t         expected_len;
} principal_case;

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

static bool principal_matches(const idl_value *value,
                              const uint8_t   *expected,
                              size_t           expected_len) {
    if (!value || value->kind != IDL_VALUE_PRINCIPAL) {
        return false;
    }
    if (value->data.principal.len != expected_len) {
        return false;
    }
    if (expected_len == 0) {
        return true;
    }
    return memcmp(value->data.principal.data, expected, expected_len) == 0;
}

int main(void) {
    const uint8_t principal_w7x7r[] = {0xca, 0xff, 0xee};
    const uint8_t principal_2chl6[] = {0xef, 0xcd, 0xab, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x01};

    const principal_case cases[] = {
        {"principal ic0",           "DIDL\\00\\01\\68\\01\\00",                 true,  NULL, 0                      },
        {"principal w7x7r",         "DIDL\\00\\01\\68\\01\\03\\ca\\ff\\ee",     true,
         principal_w7x7r,                                                                    sizeof(principal_w7x7r)},
        {"principal 2chl6",
         "DIDL\\00\\01\\68\\01\\09\\ef\\cd\\ab\\00\\00\\00\\00\\00\\01",        true,
         principal_2chl6,                                                                    sizeof(principal_2chl6)},
        {"principal no tag",        "DIDL\\00\\01\\68\\03\\ca\\ff\\ee",         false, NULL,
         0                                                                                                          },
        {"principal too short",     "DIDL\\00\\01\\68\\01\\03\\ca\\ff",         false, NULL,
         0                                                                                                          },
        {"principal too long",      "DIDL\\00\\01\\68\\01\\03\\ca\\ff\\ee\\ee",
         false,                                                                        NULL, 0                      },
        {"principal not construct", "DIDL\\01\\68\\01\\00\\01\\03\\ca\\ff\\ee",
         false,                                                                        NULL, 0                      },
    };

    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const principal_case *tc = &cases[i];
        size_t                len = 0;
        uint8_t              *data = parse_blob_string(tc->blob, &len);
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
        st = idl_deserializer_get_value_with_type(
            de, idl_type_principal(&arena), &decoded);

        idl_status done_st = IDL_STATUS_OK;
        if (st == IDL_STATUS_OK) {
            done_st = idl_deserializer_done(de);
        }

        if (tc->expect_ok) {
            if (st != IDL_STATUS_OK || done_st != IDL_STATUS_OK ||
                !principal_matches(decoded, tc->expected_bytes,
                                   tc->expected_len)) {
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
        printf("PASS: reference subset tests\n");
        return 0;
    }
    printf("FAIL: reference subset tests (%d failures)\n", failures);
    return 1;
}
