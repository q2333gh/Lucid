/**
 * Subset of reference.test.did blob cases for service/func decoding.
 */
#include "idl/runtime.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct service_case {
    const char    *name;
    const char    *blob;
    bool           expect_ok;
    const uint8_t *expected_bytes;
    size_t         expected_len;
} service_case;

typedef struct func_case {
    const char    *name;
    const char    *blob;
    bool           expect_ok;
    const uint8_t *expected_bytes;
    size_t         expected_len;
    const char    *expected_method;
} func_case;

typedef struct subtype_case {
    const char *name;
    const char *blob;
    bool        expect_ok;
    bool        is_service;
    bool        is_query;
    bool        use_opt_arg;
    bool        empty_service;
    bool        unicode_service;
} subtype_case;

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

static bool service_matches(const idl_value *value,
                            const uint8_t   *expected,
                            size_t           expected_len) {
    if (!value || value->kind != IDL_VALUE_SERVICE) {
        return false;
    }
    if (value->data.service.len != expected_len) {
        return false;
    }
    if (expected_len == 0) {
        return true;
    }
    return memcmp(value->data.service.data, expected, expected_len) == 0;
}

static bool func_matches(const idl_value *value,
                         const uint8_t   *expected,
                         size_t           expected_len,
                         const char      *method) {
    size_t method_len = method ? strlen(method) : 0;
    if (!value || value->kind != IDL_VALUE_FUNC) {
        return false;
    }
    if (value->data.func.principal_len != expected_len) {
        return false;
    }
    if (expected_len > 0 &&
        memcmp(value->data.func.principal, expected, expected_len) != 0) {
        return false;
    }
    if (value->data.func.method_len != method_len) {
        return false;
    }
    if (method_len == 0) {
        return true;
    }
    return memcmp(value->data.func.method, method, method_len) == 0;
}

static int run_service_cases(void) {
    const uint8_t principal_w7x7r[] = {0xca, 0xff, 0xee};

    const service_case cases[] = {
        {"service w7x7r",            "DIDL\\01\\69\\00\\01\\00\\01\\03\\ca\\ff\\ee", true,
         principal_w7x7r,                                                                         sizeof(principal_w7x7r)},
        {"service wrong tag",        "DIDL\\01\\69\\00\\01\\00\\00\\03\\ca\\ff\\ee",
         false,                                                                             NULL, 0                      },
        {"service unsorted",
         "DIDL\\02\\6a\\01\\71\\01\\7d\\00\\69\\02\\04foo\\32\\00\\03foo\\00"
         "\\01\\01\\01\\03\\ca\\ff\\ee",                                             false, NULL, 0                      },
        {"service unsorted by hash",
         "DIDL\\02\\6a\\01\\71\\01\\7d\\00\\69\\02\\09foobarbaz\\00\\06foobar"
         "\\00\\01\\01\\01\\03\\ca\\ff\\ee",                                         false, NULL, 0                      },
        {"service too long",
         "DIDL\\02\\6a\\01\\71\\01\\7d\\00\\69\\03\\03foo\\00\\04foo\\32\\00"
         "\\01\\01\\01\\03\\ca\\ff\\ee",                                             false, NULL, 0                      },
        {"service too short",
         "DIDL\\02\\6a\\01\\71\\01\\7d\\00\\69\\01\\03foo\\00\\04foo\\32\\00"
         "\\01\\01\\01\\03\\ca\\ff\\ee",                                             false, NULL, 0                      },
        {"service invalid unicode",
         "DIDL\\02\\6a\\01\\71\\01\\7d\\00\\69\\01\\03\\e2\\28\\a1\\00"
         "\\01\\01\\01\\03\\ca\\ff\\ee",                                             false, NULL, 0                      },
        {"service unicode",
         "DIDL\\05i\\02\\03foo\\01\\04\\f0\\9f\\90\\82\\02j\\00\\00\\01\\02j"
         "\\01\\03\\01\\04\\01\\01n|l\\00\\01\\00\\01\\00",                          true,  NULL, 0                      },
        {"service duplicate",
         "DIDL\\02\\6a\\01\\71\\01\\7d\\00\\69\\02\\03foo\\00\\03foo\\00"
         "\\01\\01\\01\\03\\ca\\ff\\ee",                                             false, NULL, 0                      },
        {"service wrong type",       "DIDL\\00\\01\\68\\01\\03\\ca\\ff\\ee",         false,
         NULL,                                                                                    0                      },
    };

    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const service_case *tc = &cases[i];
        size_t              len = 0;
        uint8_t            *data = parse_blob_string(tc->blob, &len);
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

        idl_service svc = {0};
        idl_value  *decoded = NULL;
        st = idl_deserializer_get_value_with_type(
            de, idl_type_service(&arena, &svc), &decoded);

        idl_status done_st = IDL_STATUS_OK;
        if (st == IDL_STATUS_OK) {
            done_st = idl_deserializer_done(de);
        }

        if (tc->expect_ok) {
            if (st != IDL_STATUS_OK || done_st != IDL_STATUS_OK ||
                !service_matches(decoded, tc->expected_bytes,
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

    return failures;
}

static int run_func_cases(void) {
    const uint8_t principal_w7x7r[] = {0xca, 0xff, 0xee};

    const func_case cases[] = {
        {"func a",
         "DIDL\\01\\6a\\00\\00\\00\\01\\00\\01\\01\\03\\ca\\ff\\ee\\01\\61",          true,  principal_w7x7r, sizeof(principal_w7x7r), "a"               },
        {"func unicode method",
         "DIDL\\02j\\02|}"
         "\\01\\01\\01\\01i\\00\\01\\00\\01\\01\\00\\04\\f0\\9f\\90\\82",             true,  NULL,            0,                       "\xF0\x9F\x90\x82"},
        {"func query",
         "DIDL\\01\\6a\\01\\71\\01\\7d\\01\\01\\01\\00\\01\\01\\03\\ca\\ff"
         "\\ee\\03foo",                                                               true,  principal_w7x7r, sizeof(principal_w7x7r), "foo"             },
        {"func composite query",
         "DIDL\\01\\6a\\01\\71\\01\\7d\\01\\03\\01\\00\\01\\01\\03\\ca\\ff"
         "\\ee\\03foo",                                                               true,  principal_w7x7r, sizeof(principal_w7x7r), "foo"             },
        {"func invalid utf8",
         "DIDL\\01\\6a\\00\\00\\00\\01\\00\\01\\01\\03\\ca\\ff\\ee\\02\\c3\\28",      false, NULL,            0,                       NULL              },
        {"func invalid annotation",
         "DIDL\\01\\6a\\01\\71\\01\\7d\\01\\80\\01\\01\\00\\01\\01\\03\\ca"
         "\\ff\\ee\\03foo",                                                           false, NULL,            0,                       NULL              },
        {"func not primitive",
         "DIDL\\00\\01\\6a\\01\\01\\03\\ca\\ff\\ee\\01\\61",                          false, NULL,            0,
         NULL                                                                                                                                            },
        {"func no tag",               "DIDL\\00\\01\\6a\\01\\03\\ca\\ff\\ee\\01\\61", false,
         NULL,                                                                                                0,                       NULL              },
        {"func service not in table",
         "DIDL\\01\\6a\\01\\69\\01\\7d\\00\\01\\00\\01\\01\\03\\ca\\ff"
         "\\ee\\03foo",                                                               false, NULL,            0,                       NULL              },
        {"func wrong tag",
         "DIDL\\01\\6a\\00\\00\\00\\01\\00\\01\\00\\03\\ca\\ff\\ee\\01\\61",          false, NULL,            0,                       NULL              },
    };

    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const func_case *tc = &cases[i];
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

        idl_type  *wire_type = NULL;
        idl_value *decoded = NULL;
        st = idl_deserializer_get_value(de, &wire_type, &decoded);

        idl_status done_st = IDL_STATUS_OK;
        if (st == IDL_STATUS_OK) {
            done_st = idl_deserializer_done(de);
        }

        if (tc->expect_ok) {
            const idl_type *actual_type = wire_type;
            if (actual_type && actual_type->kind == IDL_KIND_VAR) {
                actual_type = idl_type_env_trace(&de->header.env, actual_type);
            }
            if (st != IDL_STATUS_OK || done_st != IDL_STATUS_OK ||
                !actual_type || actual_type->kind != IDL_KIND_FUNC ||
                !func_matches(decoded, tc->expected_bytes, tc->expected_len,
                              tc->expected_method)) {
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

    return failures;
}

static idl_type *
build_service_type(idl_arena *arena, bool query, bool empty_service) {
    if (empty_service) {
        idl_service service = {0};
        return idl_type_service(arena, &service);
    }

    idl_type *text = idl_type_text(arena);
    idl_type *nat = idl_type_nat(arena);
    if (!text || !nat) {
        return NULL;
    }

    idl_type **args = idl_arena_alloc(arena, sizeof(idl_type *));
    idl_type **rets = idl_arena_alloc(arena, sizeof(idl_type *));
    if (!args || !rets) {
        return NULL;
    }
    args[0] = text;
    rets[0] = nat;

    idl_func_mode *modes = NULL;
    size_t         modes_len = 0;
    if (query) {
        modes = idl_arena_alloc(arena, sizeof(idl_func_mode));
        if (!modes) {
            return NULL;
        }
        modes[0] = IDL_FUNC_MODE_QUERY;
        modes_len = 1;
    }

    idl_func func = {
        .args = args,
        .args_len = 1,
        .rets = rets,
        .rets_len = 1,
        .modes = modes,
        .modes_len = modes_len,
    };
    idl_type *func_type = idl_type_func(arena, &func);
    if (!func_type) {
        return NULL;
    }

    idl_method *methods = idl_arena_alloc(arena, sizeof(idl_method));
    if (!methods) {
        return NULL;
    }
    methods[0].name = "foo";
    methods[0].type = func_type;

    idl_service service = {
        .methods = methods,
        .methods_len = 1,
    };
    return idl_type_service(arena, &service);
}

static idl_type *build_unicode_service_type(idl_arena *arena) {
    idl_type *int_type = idl_type_int(arena);
    idl_type *opt_int = idl_type_opt(arena, int_type);
    idl_type *record_empty = idl_type_record(arena, NULL, 0);
    if (!int_type || !opt_int || !record_empty) {
        return NULL;
    }

    idl_type **args_bull = idl_arena_alloc(arena, sizeof(idl_type *));
    idl_type **rets_bull = idl_arena_alloc(arena, sizeof(idl_type *));
    if (!args_bull || !rets_bull) {
        return NULL;
    }
    args_bull[0] = opt_int;
    rets_bull[0] = record_empty;

    idl_func_mode *modes_bull = idl_arena_alloc(arena, sizeof(idl_func_mode));
    if (!modes_bull) {
        return NULL;
    }
    modes_bull[0] = IDL_FUNC_MODE_QUERY;

    idl_func func_bull = {
        .args = args_bull,
        .args_len = 1,
        .rets = rets_bull,
        .rets_len = 1,
        .modes = modes_bull,
        .modes_len = 1,
    };
    idl_type *bull_type = idl_type_func(arena, &func_bull);
    if (!bull_type) {
        return NULL;
    }

    idl_func_mode *modes_foo = idl_arena_alloc(arena, sizeof(idl_func_mode));
    if (!modes_foo) {
        return NULL;
    }
    modes_foo[0] = IDL_FUNC_MODE_ONEWAY;

    idl_func func_foo = {
        .args = NULL,
        .args_len = 0,
        .rets = NULL,
        .rets_len = 0,
        .modes = modes_foo,
        .modes_len = 1,
    };
    idl_type *foo_type = idl_type_func(arena, &func_foo);
    if (!foo_type) {
        return NULL;
    }

    idl_method *methods = idl_arena_alloc(arena, sizeof(idl_method) * 2);
    if (!methods) {
        return NULL;
    }
    methods[0].name = "foo";
    methods[0].type = foo_type;
    methods[1].name = "\xF0\x9F\x90\x82";
    methods[1].type = bull_type;

    idl_service service = {
        .methods = methods,
        .methods_len = 2,
    };
    return idl_type_service(arena, &service);
}

static idl_type *build_func_type(idl_arena *arena, bool query, bool opt_arg) {
    idl_type *text = idl_type_text(arena);
    idl_type *nat = idl_type_nat(arena);
    if (!text || !nat) {
        return NULL;
    }

    idl_type *opt_text = text;
    if (opt_arg) {
        opt_text = idl_type_opt(arena, text);
        if (!opt_text) {
            return NULL;
        }
    }

    idl_type **args =
        idl_arena_alloc(arena, sizeof(idl_type *) * (opt_arg ? 2 : 1));
    idl_type **rets = idl_arena_alloc(arena, sizeof(idl_type *));
    if (!args || !rets) {
        return NULL;
    }
    args[0] = text;
    if (opt_arg) {
        args[1] = opt_text;
    }
    rets[0] = nat;

    idl_func_mode *modes = NULL;
    size_t         modes_len = 0;
    if (query) {
        modes = idl_arena_alloc(arena, sizeof(idl_func_mode));
        if (!modes) {
            return NULL;
        }
        modes[0] = IDL_FUNC_MODE_QUERY;
        modes_len = 1;
    }

    idl_func func = {
        .args = args,
        .args_len = opt_arg ? 2 : 1,
        .rets = rets,
        .rets_len = 1,
        .modes = modes,
        .modes_len = modes_len,
    };
    return idl_type_func(arena, &func);
}

static int run_subtype_cases(void) {
    const subtype_case cases[] = {
        {"service missing method",
         "DIDL\\01\\69\\00\\01\\00\\01\\03\\ca\\ff\\ee",                    false, true,  false,
         false,                                                                                         false, false},
        {"service query mismatch",
         "DIDL\\02\\6a\\01\\71\\01\\7d\\01\\01\\69\\01\\03foo\\00\\01\\01"
         "\\01\\03\\ca\\ff\\ee",                                            false, true,  false, false, false, false},
        {"service expect query",
         "DIDL\\02\\6a\\01\\71\\01\\7d\\00\\69\\01\\03foo\\00\\01\\01"
         "\\01\\03\\ca\\ff\\ee",                                            false, true,  true,  false, false, false},
        {"service decodes at empty",
         "DIDL\\02\\6a\\01\\71\\01\\7d\\00\\69\\01\\03foo\\00\\01\\01"
         "\\01\\03\\ca\\ff\\ee",                                            true,  true,  false, false, true,  false},
        {"service unicode subtype",
         "DIDL\\05i\\02\\03foo\\01\\04\\f0\\9f\\90\\82\\02j\\00\\00\\01\\02j"
         "\\01\\03\\01\\04\\01\\01n|l\\00\\01\\00\\01\\00",                 true,  true,  false, false, false, true },
        {"func missing args",
         "DIDL\\01\\6a\\00\\00\\00\\01\\00\\01\\01\\03\\ca\\ff\\ee\\03foo", false, false, false, false, false, false},
        {"func widens args",
         "DIDL\\01\\6a\\01\\71\\01\\7d\\00\\01\\00\\01\\01\\03\\ca\\ff"
         "\\ee\\03foo",                                                     true,  false, false, true,  false, false},
    };

    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const subtype_case *tc = &cases[i];
        size_t              len = 0;
        uint8_t            *data = parse_blob_string(tc->blob, &len);
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

        idl_type *expected = NULL;
        if (tc->is_service) {
            if (tc->unicode_service) {
                expected = build_unicode_service_type(&arena);
            } else {
                expected =
                    build_service_type(&arena, tc->is_query, tc->empty_service);
            }
        } else {
            expected = build_func_type(&arena, tc->is_query, tc->use_opt_arg);
        }
        if (!expected) {
            printf("FAIL: %s (type build)\n", tc->name);
            idl_arena_destroy(&arena);
            free(data);
            failures++;
            continue;
        }

        idl_value *decoded = NULL;
        st = idl_deserializer_get_value_with_type(de, expected, &decoded);
        idl_status done_st = IDL_STATUS_OK;
        if (st == IDL_STATUS_OK) {
            done_st = idl_deserializer_done(de);
        }

        if (tc->expect_ok) {
            if (st != IDL_STATUS_OK || done_st != IDL_STATUS_OK) {
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

    return failures;
}

int main(void) {
    int failures = 0;

    failures += run_service_cases();
    failures += run_func_cases();
    failures += run_subtype_cases();

    if (failures == 0) {
        printf("PASS: service/func reference subset tests\n");
        return 0;
    }
    printf("FAIL: service/func reference subset tests (%d failures)\n",
           failures);
    return 1;
}
