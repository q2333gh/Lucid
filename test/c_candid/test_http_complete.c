/**
 * Test complete http_request_args with manual type construction
 * Reproduces the exact logic from cdk-c
 */
#include "ic_candid_builder.h"
#include "idl/debug.h"
#include "idl/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Import the type builder from cdk-c
extern idl_type *build_http_request_args_type(idl_arena *arena);

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main(void) {
    printf("=== Complete http_request_args Test ===\n\n");

    idl_arena arena;
    idl_arena_init(&arena, 8192);

    // Build value (same as build_http_request_args_candid)
    printf("Building value...\n");
    idl_value_field fields[7];
    int             field_idx = 0;

    // 1. url
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("url"), .name = "url"},
        .value = idl_value_text_cstr(
            &arena, "https://jsonplaceholder.typicode.com/todos/1")
    };

    // 2. max_response_bytes: None
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("max_response_bytes"),
                  .name = "max_response_bytes"},
        .value = idl_value_opt_none(&arena)
    };

    // 3. method: variant { get } (index 0)
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

    // 4. headers
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

    // 5-7: body, transform, is_replicated = None
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

    // Sort value fields
    idl_value_fields_sort_inplace(fields, field_idx);
    idl_value *value = idl_value_record(&arena, fields, field_idx);

    printf("✓ Value created\n");
    IDL_DEBUG_PRINT("Value field count: %d", field_idx);

    // Build type using external function
    printf("\nBuilding type...\n");
    idl_type *type = build_http_request_args_type(&arena);
    if (!type) {
        printf("✗ Failed to build type\n");
        idl_arena_destroy(&arena);
        return 1;
    }
    printf("✓ Type built\n");

    // Try to add to builder
    printf("\nAdding to builder...\n");
    idl_builder builder;
    idl_builder_init(&builder, &arena);

    idl_status st = idl_builder_arg(&builder, type, value);
    if (st != IDL_STATUS_OK) {
        printf("✗ idl_builder_arg failed with status %d\n", st);
        printf("\nPossible causes:\n");
        printf("- Type and value structure mismatch\n");
        printf("- Field order mismatch\n");
        printf("- Variant index mismatch\n");
        idl_arena_destroy(&arena);
        return 1;
    }
    printf("✓ Arg added to builder\n");

    // Serialize
    printf("\nSerializing...\n");
    uint8_t *bytes;
    size_t   len;
    if (idl_builder_serialize(&builder, &bytes, &len) != IDL_STATUS_OK) {
        printf("✗ Serialization failed\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    printf("✓ Serialized %zu bytes\n\n", len);
    printf("Hex output:\n");
    print_hex(bytes, len);
    printf("\n");

    // Save for verification
    FILE *f = fopen("http_args_complete.bin", "wb");
    if (f) {
        fwrite(bytes, 1, len, f);
        fclose(f);
        printf("✓ Saved to http_args_complete.bin\n");
    }

    printf("\n✓ SUCCESS: Complete encoding works!\n");

    idl_arena_destroy(&arena);
    return 0;
}
