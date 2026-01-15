/**
 * Complex round-trip test for c_candid serialization/deserialization.
 *
 * Builds a nested record with opt/vec/variant and numeric types,
 * serializes it, then deserializes with the expected type and
 * checks decoded values.
 */
#include "ic_candid_builder.h"
#include "idl/runtime.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_true(int condition, const char *message) {
    if (!condition) {
        printf("FAIL: %s\n", message);
        failures++;
    }
}

static idl_label make_label(const char *name) {
    idl_label label;
    label.kind = IDL_LABEL_NAME;
    label.id = idl_hash(name);
    label.name = name;
    return label;
}

static const idl_value_field *find_record_field(const idl_value *record,
                                                const char     *name) {
    if (!record || record->kind != IDL_VALUE_RECORD || !name) {
        return NULL;
    }
    uint32_t target = idl_hash(name);
    for (size_t i = 0; i < record->data.record.len; i++) {
        const idl_value_field *field = &record->data.record.fields[i];
        if (field->label.id == target) {
            return field;
        }
    }
    return NULL;
}

static int find_type_field_index(const idl_field *fields,
                                 size_t          len,
                                 const char     *name) {
    uint32_t target = idl_hash(name);
    for (size_t i = 0; i < len; i++) {
        if (fields[i].label.id == target) {
            return (int)i;
        }
    }
    return -1;
}

static void expect_text(const idl_value *value, const char *expected) {
    expect_true(value && value->kind == IDL_VALUE_TEXT,
                "expected text value");
    if (!value || value->kind != IDL_VALUE_TEXT) {
        return;
    }
    size_t expected_len = strlen(expected);
    expect_true(value->data.text.len == expected_len, "text length matches");
    expect_true(strncmp(value->data.text.data, expected, expected_len) == 0,
                "text content matches");
}

static void expect_nat32(const idl_value *value, uint32_t expected) {
    expect_true(value && value->kind == IDL_VALUE_NAT32,
                "expected nat32 value");
    if (!value || value->kind != IDL_VALUE_NAT32) {
        return;
    }
    expect_true(value->data.nat32_val == expected, "nat32 matches");
}

static void expect_nat64(const idl_value *value, uint64_t expected) {
    expect_true(value && value->kind == IDL_VALUE_NAT64,
                "expected nat64 value");
    if (!value || value->kind != IDL_VALUE_NAT64) {
        return;
    }
    expect_true(value->data.nat64_val == expected, "nat64 matches");
}

static void expect_int32(const idl_value *value, int32_t expected) {
    expect_true(value && value->kind == IDL_VALUE_INT32,
                "expected int32 value");
    if (!value || value->kind != IDL_VALUE_INT32) {
        return;
    }
    expect_true(value->data.int32_val == expected, "int32 matches");
}

static void expect_int64(const idl_value *value, int64_t expected) {
    expect_true(value && value->kind == IDL_VALUE_INT64,
                "expected int64 value");
    if (!value || value->kind != IDL_VALUE_INT64) {
        return;
    }
    expect_true(value->data.int64_val == expected, "int64 matches");
}

static void expect_float32(const idl_value *value, float expected) {
    expect_true(value && value->kind == IDL_VALUE_FLOAT32,
                "expected float32 value");
    if (!value || value->kind != IDL_VALUE_FLOAT32) {
        return;
    }
    expect_true(fabsf(value->data.float32_val - expected) < 0.0001f,
                "float32 matches");
}

static void expect_float64(const idl_value *value, double expected) {
    expect_true(value && value->kind == IDL_VALUE_FLOAT64,
                "expected float64 value");
    if (!value || value->kind != IDL_VALUE_FLOAT64) {
        return;
    }
    expect_true(fabs(value->data.float64_val - expected) < 0.000001,
                "float64 matches");
}

static void expect_vec_text(const idl_value *value,
                            const char     *expected[],
                            size_t          expected_len) {
    expect_true(value && value->kind == IDL_VALUE_VEC, "expected vec value");
    if (!value || value->kind != IDL_VALUE_VEC) {
        return;
    }
    expect_true(value->data.vec.len == expected_len, "vec length matches");
    for (size_t i = 0; i < expected_len && i < value->data.vec.len; i++) {
        expect_text(value->data.vec.items[i], expected[i]);
    }
}

int main(void) {
    printf("=== Complex Candid Round-Trip Test ===\n\n");

    idl_arena arena;
    idl_arena_init(&arena, 8192);

    /* Build nested types */
    idl_field profile_fields[2] = {
        {.label = make_label("name"), .type = idl_type_text(&arena)},
        {.label = make_label("age"), .type = idl_type_nat32(&arena)},
    };
    idl_fields_sort_inplace(profile_fields, 2);
    idl_type *profile_type =
        idl_type_record(&arena, profile_fields, 2);

    idl_field address_fields[3] = {
        {.label = make_label("street"), .type = idl_type_text(&arena)},
        {.label = make_label("city"), .type = idl_type_text(&arena)},
        {.label = make_label("zip"), .type = idl_type_nat32(&arena)},
    };
    idl_fields_sort_inplace(address_fields, 3);
    idl_type *address_type =
        idl_type_record(&arena, address_fields, 3);
    idl_type *address_opt_type = idl_type_opt(&arena, address_type);

    idl_field status_fields[3] = {
        {.label = make_label("Active"), .type = idl_type_null(&arena)},
        {.label = make_label("Inactive"), .type = idl_type_null(&arena)},
        {.label = make_label("Banned"), .type = idl_type_text(&arena)},
    };
    idl_fields_sort_inplace(status_fields, 3);
    idl_type *status_type =
        idl_type_variant(&arena, status_fields, 3);

    idl_field root_fields[11] = {
        {.label = make_label("profile"), .type = profile_type},
        {.label = make_label("address"), .type = address_opt_type},
        {.label = make_label("tags"), .type = idl_type_vec(&arena,
                                                          idl_type_text(&arena))},
        {.label = make_label("status"), .type = status_type},
        {.label = make_label("score"), .type = idl_type_nat32(&arena)},
        {.label = make_label("balance"), .type = idl_type_nat64(&arena)},
        {.label = make_label("temp"), .type = idl_type_int32(&arena)},
        {.label = make_label("offset"), .type = idl_type_int64(&arena)},
        {.label = make_label("ratio"), .type = idl_type_float32(&arena)},
        {.label = make_label("pi"), .type = idl_type_float64(&arena)},
        {.label = make_label("note"), .type = idl_type_text(&arena)},
    };
    idl_fields_sort_inplace(root_fields, 11);
    idl_type *root_type = idl_type_record(&arena, root_fields, 11);

    /* Build values */
    idl_value_field profile_value_fields[2] = {
        {.label = make_label("name"),
         .value = idl_value_text_cstr(&arena, "Grace")},
        {.label = make_label("age"), .value = idl_value_nat32(&arena, 33)},
    };
    idl_value_fields_sort_inplace(profile_value_fields, 2);
    idl_value *profile_value =
        idl_value_record(&arena, profile_value_fields, 2);

    idl_value_field address_value_fields[3] = {
        {.label = make_label("street"),
         .value = idl_value_text_cstr(&arena, "Main St")},
        {.label = make_label("city"),
         .value = idl_value_text_cstr(&arena, "Lucid City")},
        {.label = make_label("zip"), .value = idl_value_nat32(&arena, 10001)},
    };
    idl_value_fields_sort_inplace(address_value_fields, 3);
    idl_value *address_value =
        idl_value_record(&arena, address_value_fields, 3);
    idl_value *address_opt_value = idl_value_opt_some(&arena, address_value);

    const char *tags_expected[] = {"developer", "admin"};
    idl_value *tag_items[2] = {
        idl_value_text_cstr(&arena, tags_expected[0]),
        idl_value_text_cstr(&arena, tags_expected[1]),
    };
    idl_value *tags_value = idl_value_vec(&arena, tag_items, 2);

    int banned_index = find_type_field_index(status_fields, 3, "Banned");
    expect_true(banned_index >= 0, "variant field index resolved");
    idl_value_field banned_field = {
        .label = make_label("Banned"),
        .value = idl_value_text_cstr(&arena, "spam"),
    };
    idl_value *status_value =
        idl_value_variant(&arena, (uint64_t)banned_index, &banned_field);

    idl_value_field root_value_fields[11] = {
        {.label = make_label("profile"), .value = profile_value},
        {.label = make_label("address"), .value = address_opt_value},
        {.label = make_label("tags"), .value = tags_value},
        {.label = make_label("status"), .value = status_value},
        {.label = make_label("score"), .value = idl_value_nat32(&arena, 100)},
        {.label = make_label("balance"),
         .value = idl_value_nat64(&arena, 1000000)},
        {.label = make_label("temp"), .value = idl_value_int32(&arena, -10)},
        {.label = make_label("offset"),
         .value = idl_value_int64(&arena, -1000000)},
        {.label = make_label("ratio"),
         .value = idl_value_float32(&arena, 0.75f)},
        {.label = make_label("pi"),
         .value = idl_value_float64(&arena, 3.14159)},
        {.label = make_label("note"),
         .value = idl_value_text_cstr(&arena, "roundtrip")},
    };
    idl_value_fields_sort_inplace(root_value_fields, 11);
    idl_value *root_value =
        idl_value_record(&arena, root_value_fields, 11);

    /* Serialize */
    idl_builder builder;
    expect_true(idl_builder_init(&builder, &arena) == IDL_STATUS_OK,
                "builder init ok");
    expect_true(idl_builder_arg(&builder, root_type, root_value) ==
                    IDL_STATUS_OK,
                "builder arg ok");

    uint8_t *bytes = NULL;
    size_t   len = 0;
    expect_true(idl_builder_serialize(&builder, &bytes, &len) ==
                    IDL_STATUS_OK,
                "serialize ok");
    expect_true(bytes && len > 0, "serialized bytes present");

    /* Deserialize with expected type */
    idl_arena decode_arena;
    idl_arena_init(&decode_arena, 8192);
    idl_deserializer *de = NULL;
    expect_true(idl_deserializer_new(bytes, len, &decode_arena, &de) ==
                    IDL_STATUS_OK,
                "deserializer new ok");

    idl_value *decoded = NULL;
    expect_true(idl_deserializer_get_value_with_type(
                    de, root_type, &decoded) == IDL_STATUS_OK,
                "deserializer get value ok");
    expect_true(idl_deserializer_is_done(de), "deserializer consumed args");
    expect_true(idl_deserializer_done(de) == IDL_STATUS_OK,
                "deserializer done ok");

    /* Validate decoded fields */
    expect_true(decoded && decoded->kind == IDL_VALUE_RECORD,
                "decoded record kind ok");
    if (decoded && decoded->kind == IDL_VALUE_RECORD) {
        const idl_value_field *field = NULL;

        field = find_record_field(decoded, "profile");
        expect_true(field && field->value, "profile field present");
        if (field && field->value) {
            const idl_value_field *profile_name =
                find_record_field(field->value, "name");
            const idl_value_field *profile_age =
                find_record_field(field->value, "age");
            expect_text(profile_name ? profile_name->value : NULL, "Grace");
            expect_nat32(profile_age ? profile_age->value : NULL, 33);
        }

        field = find_record_field(decoded, "address");
        expect_true(field && field->value && field->value->kind == IDL_VALUE_OPT,
                    "address opt field present");
        if (field && field->value && field->value->kind == IDL_VALUE_OPT) {
            idl_value *addr = field->value->data.opt;
            expect_true(addr && addr->kind == IDL_VALUE_RECORD,
                        "address opt some record");
            if (addr && addr->kind == IDL_VALUE_RECORD) {
                expect_text(find_record_field(addr, "street")
                                ? find_record_field(addr, "street")->value
                                : NULL,
                            "Main St");
                expect_text(find_record_field(addr, "city")
                                ? find_record_field(addr, "city")->value
                                : NULL,
                            "Lucid City");
                expect_nat32(find_record_field(addr, "zip")
                                 ? find_record_field(addr, "zip")->value
                                 : NULL,
                             10001);
            }
        }

        field = find_record_field(decoded, "tags");
        expect_true(field && field->value, "tags field present");
        if (field && field->value) {
            expect_vec_text(field->value, tags_expected, 2);
        }

        field = find_record_field(decoded, "status");
        expect_true(field && field->value &&
                        field->value->kind == IDL_VALUE_VARIANT,
                    "status variant present");
        if (field && field->value &&
            field->value->kind == IDL_VALUE_VARIANT) {
            expect_true(field->value->data.record.variant_index ==
                            (uint64_t)banned_index,
                        "variant index matches");
            const idl_value_field *variant_field =
                &field->value->data.record.fields[0];
            expect_true(variant_field->label.id == idl_hash("Banned"),
                        "variant label matches");
            expect_text(variant_field->value, "spam");
        }

        expect_nat32(find_record_field(decoded, "score")
                         ? find_record_field(decoded, "score")->value
                         : NULL,
                     100);
        expect_nat64(find_record_field(decoded, "balance")
                         ? find_record_field(decoded, "balance")->value
                         : NULL,
                     1000000);
        expect_int32(find_record_field(decoded, "temp")
                         ? find_record_field(decoded, "temp")->value
                         : NULL,
                     -10);
        expect_int64(find_record_field(decoded, "offset")
                         ? find_record_field(decoded, "offset")->value
                         : NULL,
                     -1000000);
        expect_float32(find_record_field(decoded, "ratio")
                           ? find_record_field(decoded, "ratio")->value
                           : NULL,
                       0.75f);
        expect_float64(find_record_field(decoded, "pi")
                           ? find_record_field(decoded, "pi")->value
                           : NULL,
                       3.14159);
        expect_text(find_record_field(decoded, "note")
                        ? find_record_field(decoded, "note")->value
                        : NULL,
                    "roundtrip");
    }

    idl_arena_destroy(&decode_arena);
    idl_arena_destroy(&arena);

    if (failures == 0) {
        printf("\nPASS: Complex round-trip test passed\n");
    } else {
        printf("\nFAIL: Complex round-trip test failed (%d failures)\n",
               failures);
    }
    return failures == 0 ? 0 : 1;
}
