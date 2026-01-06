/**
 * Simple test: encode a basic record without complex types
 * to verify basic Candid encoding works
 */
#include "idl/debug.h"
#include "idl/runtime.h"
#include <stdio.h>
#include <string.h>

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main(void) {
    printf("=== Simple Record Encoding Test ===\n\n");

    idl_arena arena;
    idl_arena_init(&arena, 4096);

    idl_builder builder;
    idl_builder_init(&builder, &arena);

    // Test 1: Single text argument
    printf("Test 1: Single text\n");
    idl_builder_arg_text_cstr(&builder, "hello");

    uint8_t *bytes;
    size_t   len;
    if (idl_builder_serialize(&builder, &bytes, &len) == IDL_STATUS_OK) {
        printf("✓ Encoded %zu bytes: ", len);
        print_hex(bytes, len);
    } else {
        printf("✗ Failed\n");
    }

    // Test 2: Record with name and age
    printf("\nTest 2: Simple record\n");
    idl_arena_destroy(&arena);
    idl_arena_init(&arena, 4096);
    idl_builder_init(&builder, &arena);

    // Build type
    idl_field type_fields[2];
    type_fields[0].label.kind = IDL_LABEL_NAME;
    type_fields[0].label.id = idl_hash("name");
    type_fields[0].label.name = "name";
    type_fields[0].type = idl_type_text(&arena);

    type_fields[1].label.kind = IDL_LABEL_NAME;
    type_fields[1].label.id = idl_hash("age");
    type_fields[1].label.name = "age";
    type_fields[1].type = idl_type_nat32(&arena);

    // Sort by label id
    if (type_fields[0].label.id > type_fields[1].label.id) {
        idl_field tmp = type_fields[0];
        type_fields[0] = type_fields[1];
        type_fields[1] = tmp;
    }

    idl_type *record_type = idl_type_record(&arena, type_fields, 2);

    // Build value (in same order as type)
    idl_value_field value_fields[2];
    if (type_fields[0].label.id == idl_hash("age")) {
        value_fields[0].label = type_fields[0].label;
        value_fields[0].value = idl_value_nat32(&arena, 25);
        value_fields[1].label = type_fields[1].label;
        value_fields[1].value = idl_value_text_cstr(&arena, "Alice");
    } else {
        value_fields[0].label = type_fields[0].label;
        value_fields[0].value = idl_value_text_cstr(&arena, "Alice");
        value_fields[1].label = type_fields[1].label;
        value_fields[1].value = idl_value_nat32(&arena, 25);
    }

    idl_value *record_value = idl_value_record(&arena, value_fields, 2);

    IDL_DEBUG_PRINT("Adding record to builder...");
    if (idl_builder_arg(&builder, record_type, record_value) == IDL_STATUS_OK) {
        IDL_DEBUG_PRINT("Serializing...");
        if (idl_builder_serialize(&builder, &bytes, &len) == IDL_STATUS_OK) {
            printf("✓ Encoded %zu bytes: ", len);
            print_hex(bytes, len);

            // Save to file
            FILE *f = fopen("simple_record.bin", "wb");
            if (f) {
                fwrite(bytes, 1, len, f);
                fclose(f);
                printf("✓ Saved to simple_record.bin\n");
                printf("\nVerify with: didc decode < simple_record.bin\n");
            }
        } else {
            printf("✗ Serialization failed\n");
        }
    } else {
        printf("✗ Failed to add arg\n");
    }

    idl_arena_destroy(&arena);
    return 0;
}
