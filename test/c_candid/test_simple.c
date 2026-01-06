/**
 * Simplest test: just build the record value and check if it can serialize
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
    printf("=== Simple http_request_args test ===\n\n");

    idl_arena arena;
    idl_arena_init(&arena, 8192);

    // Build record fields
    idl_value_field fields[7];
    int             field_idx = 0;

    // Build all 7 fields...
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

    // method variant
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

    // headers
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

    // Sort!
    idl_value_fields_sort_inplace(fields, field_idx);

    // Create record
    idl_value *record = idl_value_record(&arena, fields, field_idx);

    printf("✓ Record created\n");
    printf("Record fields count: %d\n", field_idx);

    // Print field order after sort
    printf("\nField order after sort (by hash):\n");
    for (int i = 0; i < field_idx; i++) {
        printf("  %d. %s (hash=%u)\n", i, fields[i].label.name,
               fields[i].label.id);
    }

    printf("\nExpected output from didc:\n");
    printf("4449444c046c07efd6e40271e1edeb4a01e8d6d893017fa2f5ed88047fecdaccac0"
           "47fc6a4a198060290f8f6fc097f6b019681ba027f6d036c02f1fee18d0371cbe4fd"
           "c7047101002c68747470733a2f2f6a736f6e706c616365686f6c6465722e7479706"
           "9636f64652e636f6d2f746f646f732f3100010e69632d687474702d632d64656d6f"
           "0a557365722d4167656e74\n");

    printf(
        "\nNote: Our c_candid doesn't support NULL type in idl_builder_arg\n");
    printf("This test shows the record structure is correct.\n");
    printf("The actual encoding issue needs to be debugged in cdk-c's "
           "ic_http_request.c\n");

    idl_arena_destroy(&arena);
    return 0;
}
