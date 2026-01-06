/**
 * Test type inference from value
 */
#include "ic_candid_builder.h"
#include "idl/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main(void) {
    printf("=== Testing type inference and full encoding ===\n\n");

    idl_arena arena;
    idl_arena_init(&arena, 8192);

    // Build the same record as before
    idl_value_field fields[7];
    int             field_idx = 0;

    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("url"), .name = "url"},
        .value = idl_value_text_cstr(
            &arena, "https://jsonplaceholder.typicode.com/todos/1")
    };

    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("max_response_bytes"),
                  .name = "max_response_bytes"},
        .value = idl_value_opt_none(&arena)
    };

    idl_value_field method_field = {
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("get"), .name = "get"},
        .value = idl_value_null(&arena)
    };
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("method"),
                  .name = "method"},
        .value = idl_value_variant(&arena, 0, &method_field)
    };

    idl_value_field header_fields[2];
    header_fields[0] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("name"),
                  .name = "name"},
        .value = idl_value_text_cstr(&arena, "User-Agent")
    };
    header_fields[1] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("value"),
                  .name = "value"},
        .value = idl_value_text_cstr(&arena, "ic-http-c-demo")
    };
    idl_value_fields_sort_inplace(header_fields, 2);

    idl_value *header_rec = idl_value_record(&arena, header_fields, 2);
    idl_value *headers_vec = idl_value_vec(&arena, &header_rec, 1);

    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("headers"),
                  .name = "headers"},
        .value = headers_vec
    };

    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("body"),
                  .name = "body"},
        .value = idl_value_opt_none(&arena)
    };

    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("transform"),
                  .name = "transform"},
        .value = idl_value_opt_none(&arena)
    };

    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("is_replicated"),
                  .name = "is_replicated"},
        .value = idl_value_opt_none(&arena)
    };

    idl_value_fields_sort_inplace(fields, field_idx);
    idl_value *record = idl_value_record(&arena, fields, field_idx);

    printf("✓ Record value created\n");

    // Test type inference
    printf("\nTesting idl_type_from_value...\n");
    idl_type *rec_type = idl_type_from_value(&arena, record);
    if (!rec_type) {
        printf("✗ FAIL: idl_type_from_value returned NULL\n");
        idl_arena_destroy(&arena);
        return 1;
    }
    printf("✓ Type inference successful\n");

    // Try full serialization
    printf("\nTrying full serialization...\n");
    idl_builder builder;
    if (idl_builder_init(&builder, &arena) != IDL_STATUS_OK) {
        printf("✗ FAIL: idl_builder_init failed\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    printf("Adding arg to builder...\n");
    idl_status st = idl_builder_arg(&builder, rec_type, record);
    if (st != IDL_STATUS_OK) {
        printf("✗ FAIL: idl_builder_arg failed with status %d\n", st);
        idl_arena_destroy(&arena);
        return 1;
    }
    printf("✓ Arg added to builder\n");

    printf("\nSerializing...\n");
    uint8_t *bytes;
    size_t   len;
    if (idl_builder_serialize(&builder, &bytes, &len) != IDL_STATUS_OK) {
        printf("✗ FAIL: Serialization failed\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    printf("✓ Serialization successful (%zu bytes)\n\n", len);
    printf("Our encoding:\n");
    print_hex(bytes, len);

    printf("\nExpected from didc:\n");
    printf("4449444c046c07efd6e40271e1edeb4a01e8d6d893017fa2f5ed88047fecdaccac0"
           "47fc6a4a198060290f8f6fc097f6b019681ba027f6d036c02f1fee18d0371cbe4fd"
           "c7047101002c68747470733a2f2f6a736f6e706c616365686f6c6465722e7479706"
           "9636f64652e636f6d2f746f646f732f3100010e69632d687474702d632d64656d6f"
           "0a557365722d4167656e74\n");

    idl_arena_destroy(&arena);
    return 0;
}
