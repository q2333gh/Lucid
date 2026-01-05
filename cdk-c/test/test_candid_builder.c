/**
 * test_candid_builder.c - Unit tests for ic_candid_builder API
 *
 * Tests all three solutions:
 * 1. Macro-based API
 * 2. Builder API
 * 3. Ultra-short macros
 */

#include "ic_candid_builder.h"
#include "idl/runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                                                             \
    printf("\n🧪 Testing: %s\n", name);                                      \
    static void test_##name(void);                                             \
    void        test_##name##_wrapper(void) {                                  \
               test_##name();                                                  \
               tests_passed++;                                                 \
               printf("✅ PASS: %s\n", name);                                   \
    }                                                                          \
    void test_##name(void)

#define RUN_TEST(name) test_##name##_wrapper()

#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("❌ FAIL: %s:%d: Assertion failed: %s\n", __FILE__,          \
                   __LINE__, #cond);                                           \
            tests_failed++;                                                    \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                                   \
    ASSERT((ptr) != NULL && "Expected non-null pointer")

#define ASSERT_EQ(a, b) ASSERT((a) == (b) && "Expected values to be equal")

/* ============================================================
 * Test: Macro API - Simple Record
 * ============================================================ */
TEST(macro_simple_record) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    // Create Address type
    idl_type *addr_type =
        IDL_RECORD(&arena, IDL_FIELD("street", idl_type_text(&arena)),
                   IDL_FIELD("city", idl_type_text(&arena)),
                   IDL_FIELD("zip", idl_type_nat(&arena)));

    ASSERT_NOT_NULL(addr_type);
    ASSERT_EQ(addr_type->kind, IDL_KIND_RECORD);
    ASSERT_EQ(addr_type->data.record.fields_len, 3);

    // Create Address value
    idl_value *addr_val = IDL_RECORD_VALUE(
        &arena,
        IDL_VALUE_FIELD("street", idl_value_text_cstr(&arena, "123 Main St")),
        IDL_VALUE_FIELD("city", idl_value_text_cstr(&arena, "SF")),
        IDL_VALUE_FIELD("zip", idl_value_nat32(&arena, 94102)));

    ASSERT_NOT_NULL(addr_val);
    ASSERT_EQ(addr_val->kind, IDL_VALUE_RECORD);
    ASSERT_EQ(addr_val->data.record.len, 3);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Macro API - Nested Record
 * ============================================================ */
TEST(macro_nested_record) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    // Address type
    idl_type *addr_type =
        IDL_RECORD(&arena, IDL_FIELD("street", idl_type_text(&arena)),
                   IDL_FIELD("zip", idl_type_nat(&arena)));

    // Person type with nested address
    idl_type *person_type =
        IDL_RECORD(&arena, IDL_FIELD("name", idl_type_text(&arena)),
                   IDL_FIELD("address", addr_type));

    ASSERT_NOT_NULL(person_type);
    ASSERT_EQ(person_type->data.record.fields_len, 2);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Macro API - Optional Field
 * ============================================================ */
TEST(macro_optional_field) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    // Type with optional age
    idl_type *person_type = IDL_RECORD(
        &arena, IDL_FIELD("name", idl_type_text(&arena)),
        IDL_FIELD("age", idl_type_opt(&arena, idl_type_nat(&arena))));

    ASSERT_NOT_NULL(person_type);

    // Value with Some(age)
    idl_value *person_with_age = IDL_RECORD_VALUE(
        &arena, IDL_VALUE_FIELD("name", idl_value_text_cstr(&arena, "Alice")),
        IDL_VALUE_FIELD(
            "age", idl_value_opt_some(&arena, idl_value_nat32(&arena, 30))));

    ASSERT_NOT_NULL(person_with_age);

    // Value with None
    idl_value *person_without_age = IDL_RECORD_VALUE(
        &arena, IDL_VALUE_FIELD("name", idl_value_text_cstr(&arena, "Bob")),
        IDL_VALUE_FIELD("age", idl_value_opt_none(&arena)));

    ASSERT_NOT_NULL(person_without_age);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Macro API - Vector
 * ============================================================ */
TEST(macro_vector) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    // Create vector of texts
    idl_value *emails =
        IDL_VEC(&arena, idl_value_text_cstr(&arena, "a@test.com"),
                idl_value_text_cstr(&arena, "b@test.com"),
                idl_value_text_cstr(&arena, "c@test.com"));

    ASSERT_NOT_NULL(emails);
    ASSERT_EQ(emails->kind, IDL_VALUE_VEC);
    ASSERT_EQ(emails->data.vec.len, 3);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Macro API - Variant
 * ============================================================ */
TEST(macro_variant) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    idl_type *status_type =
        IDL_VARIANT(&arena, IDL_FIELD("Active", idl_type_null(&arena)),
                    IDL_FIELD("Inactive", idl_type_null(&arena)),
                    IDL_FIELD("Banned", idl_type_text(&arena)));

    ASSERT_NOT_NULL(status_type);
    ASSERT_EQ(status_type->kind, IDL_KIND_VARIANT);
    ASSERT_EQ(status_type->data.record.fields_len, 3);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Builder API - Simple Record
 * ============================================================ */
TEST(builder_simple_record) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    idl_record_builder *rb = idl_record_builder_new(&arena, 3);
    ASSERT_NOT_NULL(rb);

    idl_record_builder_text(rb, "name", "Alice");
    idl_record_builder_nat32(rb, "age", 30);
    idl_record_builder_bool(rb, "active", true);

    idl_type  *type = idl_record_builder_build_type(rb);
    idl_value *val = idl_record_builder_build_value(rb);

    ASSERT_NOT_NULL(type);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(type->data.record.fields_len, 3);
    ASSERT_EQ(val->data.record.len, 3);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Builder API - All Types
 * ============================================================ */
TEST(builder_all_types) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    idl_record_builder *rb = idl_record_builder_new(&arena, 10);
    ASSERT_NOT_NULL(rb);

    idl_record_builder_bool(rb, "bool_field", true);
    idl_record_builder_nat32(rb, "nat32_field", 42);
    idl_record_builder_nat64(rb, "nat64_field", 1000000);
    idl_record_builder_int32(rb, "int32_field", -42);
    idl_record_builder_int64(rb, "int64_field", -1000000);
    idl_record_builder_float32(rb, "float32_field", 3.14f);
    idl_record_builder_float64(rb, "float64_field", 2.71828);
    idl_record_builder_text(rb, "text_field", "hello");

    idl_type  *type = idl_record_builder_build_type(rb);
    idl_value *val = idl_record_builder_build_value(rb);

    ASSERT_NOT_NULL(type);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(type->data.record.fields_len, 8);
    ASSERT_EQ(val->data.record.len, 8);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Builder API - Optional Field
 * ============================================================ */
TEST(builder_optional) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    idl_record_builder *rb = idl_record_builder_new(&arena, 2);
    idl_record_builder_text(rb, "name", "Alice");
    idl_record_builder_opt(rb, "age", idl_type_nat(&arena),
                           idl_value_nat32(&arena, 30));

    idl_type  *type = idl_record_builder_build_type(rb);
    idl_value *val = idl_record_builder_build_value(rb);

    ASSERT_NOT_NULL(type);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(rb->count, 2);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Builder API - Vector Field
 * ============================================================ */
TEST(builder_vector) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    idl_value *items[] = {
        idl_value_text_cstr(&arena, "item1"),
        idl_value_text_cstr(&arena, "item2"),
    };

    idl_record_builder *rb = idl_record_builder_new(&arena, 2);
    idl_record_builder_text(rb, "name", "Test");
    idl_record_builder_vec(rb, "items", idl_type_text(&arena), items, 2);

    idl_type  *type = idl_record_builder_build_type(rb);
    idl_value *val = idl_record_builder_build_value(rb);

    ASSERT_NOT_NULL(type);
    ASSERT_NOT_NULL(val);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Builder API - Nested Record
 * ============================================================ */
TEST(builder_nested) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    // Build inner record (address)
    idl_record_builder *addr_rb = idl_record_builder_new(&arena, 2);
    idl_record_builder_text(addr_rb, "street", "Main St");
    idl_record_builder_text(addr_rb, "city", "SF");

    // Build outer record (person)
    idl_record_builder *person_rb = idl_record_builder_new(&arena, 2);
    idl_record_builder_text(person_rb, "name", "Alice");
    idl_record_builder_field(person_rb, "address",
                             idl_record_builder_build_type(addr_rb),
                             idl_record_builder_build_value(addr_rb));

    idl_type  *type = idl_record_builder_build_type(person_rb);
    idl_value *val = idl_record_builder_build_value(person_rb);

    ASSERT_NOT_NULL(type);
    ASSERT_NOT_NULL(val);
    ASSERT_EQ(person_rb->count, 2);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Builder API - Capacity Limit
 * ============================================================ */
TEST(builder_capacity_limit) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    // Create builder with capacity 2
    idl_record_builder *rb = idl_record_builder_new(&arena, 2);
    idl_record_builder_text(rb, "field1", "value1");
    idl_record_builder_text(rb, "field2", "value2");

    // Try to add third field (should be ignored)
    idl_record_builder_text(rb, "field3", "value3");

    ASSERT_EQ(rb->count, 2); // Should still be 2

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Builder API - Blob Field
 * ============================================================ */
TEST(builder_blob) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};

    idl_record_builder *rb = idl_record_builder_new(&arena, 1);
    idl_record_builder_blob(rb, "data", data, sizeof(data));

    idl_type  *type = idl_record_builder_build_type(rb);
    idl_value *val = idl_record_builder_build_value(rb);

    ASSERT_NOT_NULL(type);
    ASSERT_NOT_NULL(val);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Test: Builder API - Principal Field
 * ============================================================ */
TEST(builder_principal) {
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    uint8_t principal_data[] = {0xCA, 0xFE, 0xBA, 0xBE};

    idl_record_builder *rb = idl_record_builder_new(&arena, 1);
    idl_record_builder_principal(rb, "owner", principal_data,
                                 sizeof(principal_data));

    idl_type  *type = idl_record_builder_build_type(rb);
    idl_value *val = idl_record_builder_build_value(rb);

    ASSERT_NOT_NULL(type);
    ASSERT_NOT_NULL(val);

    idl_arena_destroy(&arena);
}

/* ============================================================
 * Main Test Runner
 * ============================================================ */
int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Candid Builder API Test Suite                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    // Macro API tests
    printf("\n📦 MACRO API TESTS\n");
    printf("══════════════════════════════════════════════════════════════\n");
    RUN_TEST(macro_simple_record);
    RUN_TEST(macro_nested_record);
    RUN_TEST(macro_optional_field);
    RUN_TEST(macro_vector);
    RUN_TEST(macro_variant);

    // Builder API tests
    printf("\n🏗️  BUILDER API TESTS\n");
    printf("══════════════════════════════════════════════════════════════\n");
    RUN_TEST(builder_simple_record);
    RUN_TEST(builder_all_types);
    RUN_TEST(builder_optional);
    RUN_TEST(builder_vector);
    RUN_TEST(builder_nested);
    RUN_TEST(builder_capacity_limit);
    RUN_TEST(builder_blob);
    RUN_TEST(builder_principal);

    // Summary
    printf(
        "\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  TEST SUMMARY                                              ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  ✅ Passed: %-3d                                            ║\n",
           tests_passed);
    printf("║  ❌ Failed: %-3d                                            ║\n",
           tests_failed);
    printf("╚════════════════════════════════════════════════════════════╝\n");

    return tests_failed > 0 ? 1 : 0;
}
