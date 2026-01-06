/**
 * Simple test: directly use cdk-c's build_http_request_args_candid
 * to verify the Candid encoding
 */
#include "ic_candid_builder.h"
#include "ic_http_request.h"
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
    printf("=== Testing cdk-c http_request_args Candid encoding ===\n\n");

    // Setup test args
    ic_http_request_args_t args;
    ic_http_request_args_init(&args,
                              "https://jsonplaceholder.typicode.com/todos/1");

    // Add header
    static ic_http_header_t headers[1];
    headers[0].name = "User-Agent";
    headers[0].value = "ic-http-c-demo";
    args.headers = headers;
    args.headers_count = 1;

    args.max_response_bytes = 0; // Will default, making it None in Candid
    args.transform = NULL;
    // Don't set is_replicated explicitly to keep it as None

    // Initialize arena
    idl_arena arena;
    if (idl_arena_init(&arena, 8192) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to init arena\n");
        return 1;
    }

    // Build Candid value manually (same logic as ic_http_request.c)
    idl_value_field fields[7];
    int             field_idx = 0;

    // 1. url: text
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("url"), .name = "url"},
        .value = idl_value_text_cstr(&arena, args.url)
    };

    // 2. max_response_bytes: opt nat64 = None
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("max_response_bytes"),
                  .name = "max_response_bytes"},
        .value = idl_value_opt_none(&arena)
    };

    // 3. method: variant { get; head; post } = get (index 0)
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

    // 4. headers: vec http_header
    idl_value_field header_fields[2];
    header_fields[0] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("name"),
                  .name = "name"},
        .value = idl_value_text_cstr(&arena, headers[0].name)
    };
    header_fields[1] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("value"),
                  .name = "value"},
        .value = idl_value_text_cstr(&arena, headers[0].value)
    };

    idl_value_fields_sort_inplace(header_fields, 2);

    idl_value *header_record = idl_value_record(&arena, header_fields, 2);
    idl_value *headers_vec = idl_value_vec(&arena, &header_record, 1);

    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("headers"),
                  .name = "headers"},
        .value = headers_vec
    };

    // 5. body: opt blob = None
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("body"),
                  .name = "body"},
        .value = idl_value_opt_none(&arena)
    };

    // 6. transform: opt record = None
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("transform"),
                  .name = "transform"},
        .value = idl_value_opt_none(&arena)
    };

    // 7. is_replicated: opt bool = None
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("is_replicated"),
                  .name = "is_replicated"},
        .value = idl_value_opt_none(&arena)
    };

    // Sort all fields by label id (CRITICAL!)
    idl_value_fields_sort_inplace(fields, field_idx);

    // Create record
    idl_value *arg_value = idl_value_record(&arena, fields, field_idx);

    printf("✓ Successfully built Candid value\n\n");

    // Serialize
    idl_builder builder;
    if (idl_builder_init(&builder, &arena) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to init builder\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    if (idl_builder_arg(&builder, NULL, arg_value) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to add arg to builder\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    uint8_t *candid_bytes;
    size_t   candid_len;
    if (idl_builder_serialize(&builder, &candid_bytes, &candid_len) !=
        IDL_STATUS_OK) {
        fprintf(stderr, "Failed to serialize\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    printf("Our C encoding (%zu bytes):\n", candid_len);
    print_hex(candid_bytes, candid_len);
    printf("\n");

    // Expected from didc
    const char *expected_hex =
        "4449444c046c07efd6e40271e1edeb4a01e8d6d893017fa2f5ed88047fecdaccac047f"
        "c6a4a198060290f8f6fc097f6b019681ba027f6d036c02f1fee18d0371cbe4fdc70471"
        "01002c68747470733a2f2f6a736f6e706c616365686f6c6465722e74797069636f6465"
        "2e636f6d2f746f646f732f3100010e69632d687474702d632d64656d6f0a557365722d"
        "4167656e74";

    // Convert expected hex to bytes for comparison
    size_t   expected_len = strlen(expected_hex) / 2;
    uint8_t *expected_bytes = malloc(expected_len);
    for (size_t i = 0; i < expected_len; i++) {
        sscanf(&expected_hex[i * 2], "%2hhx", &expected_bytes[i]);
    }

    printf("Expected from didc (%zu bytes):\n", expected_len);
    print_hex(expected_bytes, expected_len);
    printf("\n");

    // Compare
    if (candid_len == expected_len &&
        memcmp(candid_bytes, expected_bytes, candid_len) == 0) {
        printf("✓ SUCCESS: Encodings match perfectly!\n");
        free(expected_bytes);
        idl_arena_destroy(&arena);
        return 0;
    } else {
        printf("✗ FAIL: Encodings differ!\n");
        printf("Length: %zu vs %zu (expected)\n", candid_len, expected_len);

        if (candid_len != expected_len) {
            printf("Length mismatch!\n");
        } else {
            printf("Byte differences:\n");
            for (size_t i = 0; i < candid_len && i < expected_len; i++) {
                if (candid_bytes[i] != expected_bytes[i]) {
                    printf("  Byte %zu: got 0x%02x, expected 0x%02x\n", i,
                           candid_bytes[i], expected_bytes[i]);
                    if (i > 10) {
                        printf("  ... (showing first few only)\n");
                        break;
                    }
                }
            }
        }

        free(expected_bytes);
        idl_arena_destroy(&arena);
        return 1;
    }
}
