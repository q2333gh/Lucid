/**
 * Test Candid encoding for http_request_args
 * Compare our C encoding with didc's expected output
 *
 * This directly tests the logic from ic_http_request.c
 */
#include "ic_candid_builder.h"
#include "idl/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    char *value;
} http_header_t;

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main(void) {
    printf("=== Testing http_request_args Candid encoding ===\n\n");

    idl_arena arena;
    if (idl_arena_init(&arena, 8192) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to init arena\n");
        return 1;
    }

    // Test data (matching our C code and didc test)
    const char   *url = "https://jsonplaceholder.typicode.com/todos/1";
    http_header_t headers[1] = {
        {.name = "User-Agent", .value = "ic-http-c-demo"}
    };

    // Build record fields
    idl_value_field fields[7];
    int             field_idx = 0;

    // 1. url: text
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("url"), .name = "url"},
        .value = idl_value_text_cstr(&arena, url)
    };

    // 2. max_response_bytes: opt nat64 = None
    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("max_response_bytes"),
                  .name = "max_response_bytes"},
        .value = idl_value_opt_none(&arena)
    };

    // 3. method: variant { get; head; post } = get (index 0)
    idl_value_field method_variant_field = {
        .label = {.kind = IDL_LABEL_NAME, .id = idl_hash("get"), .name = "get"},
        .value = idl_value_null(&arena)
    };

    fields[field_idx++] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("method"),
                  .name = "method"},
        .value = idl_value_variant(&arena, 0, &method_variant_field)
    };

    // 4. headers: vec http_header
    idl_value_field header_rec_fields[2];
    header_rec_fields[0] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("name"),
                  .name = "name"},
        .value = idl_value_text_cstr(&arena, headers[0].name)
    };
    header_rec_fields[1] = (idl_value_field){
        .label = {.kind = IDL_LABEL_NAME,
                  .id = idl_hash("value"),
                  .name = "value"},
        .value = idl_value_text_cstr(&arena, headers[0].value)
    };

    idl_value_fields_sort_inplace(header_rec_fields, 2);

    idl_value *header_record = idl_value_record(&arena, header_rec_fields, 2);
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
    idl_value *record = idl_value_record(&arena, fields, field_idx);

    // Build and serialize
    idl_builder builder;
    if (idl_builder_init(&builder, &arena) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to init builder\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    // Add argument (NULL type = infer from value)
    if (idl_builder_arg(&builder, NULL, record) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to add arg to builder\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    // Serialize to hex
    char  *hex;
    size_t hex_len;
    if (idl_builder_serialize_hex(&builder, &hex, &hex_len) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to serialize\n");
        idl_arena_destroy(&arena);
        return 1;
    }

    printf("Our C encoding (hex, %zu chars):\n%s\n\n", hex_len, hex);

    // Also print binary
    uint8_t *bytes;
    size_t   len;
    if (idl_builder_serialize(&builder, &bytes, &len) == IDL_STATUS_OK) {
        printf("Binary (%zu bytes):\n", len);
        print_hex(bytes, len);
    }

    printf("\n");
    const char *expected =
        "4449444c046c07efd6e40271e1edeb4a01e8d6d893017fa2f5ed88047fecdaccac047f"
        "c6a4a198060290f8f6fc097f6b019681ba027f6d036c02f1fee18d0371cbe4fdc70471"
        "01002c68747470733a2f2f6a736f6e706c616365686f6c6465722e74797069636f6465"
        "2e636f6d2f746f646f732f3100010e69632d687474702d632d64656d6f0a557365722d"
        "4167656e74";
    printf("Expected from didc (%zu chars):\n%s\n\n", strlen(expected),
           expected);

    if (strcmp(hex, expected) == 0) {
        printf("✓ SUCCESS: Encodings match!\n");
    } else {
        printf("✗ FAIL: Encodings differ!\n");
        printf("\nDifferences:\n");
        size_t min_len =
            hex_len < strlen(expected) ? hex_len : strlen(expected);
        for (size_t i = 0; i < min_len; i++) {
            if (hex[i] != expected[i]) {
                printf("  Position %zu: got '%c' (0x%02x), expected '%c' "
                       "(0x%02x)\n",
                       i, hex[i], hex[i], expected[i], expected[i]);
                if (i > 5) {
                    printf("  ... (showing first few differences only)\n");
                    break;
                }
            }
        }
        if (hex_len != strlen(expected)) {
            printf("  Length differs: %zu vs %zu\n", hex_len, strlen(expected));
        }
    }

    idl_arena_destroy(&arena);
    return strcmp(hex, expected) == 0 ? 0 : 1;
}
