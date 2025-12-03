/**
 * basic_usage.c - Example of using the C Candid runtime library
 *
 * This example demonstrates how to:
 * 1. Initialize the arena allocator
 * 2. Build Candid messages with various types
 * 3. Serialize to binary format
 * 4. Deserialize back to values
 *
 * Compile:
 *   cd build && cmake .. && make
 *   clang -I../runtime/runtime/include -L./lib \
 *         ../examples/basic_usage.c -lcandid_runtime -o basic_usage
 *
 * Or add to CMakeLists.txt as a target.
 */

#include <stdio.h>
#include <string.h>

#include "idl/runtime.h"

/* Helper: print hex bytes */
static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

/* Example 1: Encode simple primitives */
static void example_primitives(void) {
    printf("=== Example 1: Primitives ===\n");

    idl_arena arena;
    idl_arena_init(&arena, 4096);

    idl_builder builder;
    idl_builder_init(&builder, &arena);

    /* Add arguments: (true, 42, "hello") */
    idl_builder_arg_bool(&builder, 1);
    idl_builder_arg_nat64(&builder, 42);
    idl_builder_arg_text(&builder, "hello", 5);

    /* Serialize to binary */
    uint8_t *bytes;
    size_t   len;
    if (idl_builder_serialize(&builder, &bytes, &len) == IDL_STATUS_OK) {
        printf("Encoded: ");
        print_hex(bytes, len);
    }

    /* Also get hex string */
    char  *hex;
    size_t hex_len;
    if (idl_builder_serialize_hex(&builder, &hex, &hex_len) == IDL_STATUS_OK) {
        printf("Hex: %s\n", hex);
    }

    idl_arena_destroy(&arena);
    printf("\n");
}

/* Example 2: Encode a record */
static void example_record(void) {
    printf("=== Example 2: Record ===\n");

    idl_arena arena;
    idl_arena_init(&arena, 4096);

    idl_builder builder;
    idl_builder_init(&builder, &arena);

    /* Build record type: { name: text; age: nat32 } */
    idl_field fields[2];

    /* Field: name */
    fields[0].label.kind = IDL_LABEL_NAME;
    fields[0].label.id = idl_hash("name");
    fields[0].label.name = "name";
    fields[0].type = idl_type_text(&arena);

    /* Field: age */
    fields[1].label.kind = IDL_LABEL_NAME;
    fields[1].label.id = idl_hash("age");
    fields[1].label.name = "age";
    fields[1].type = idl_type_nat32(&arena);

    idl_type *record_type = idl_type_record(&arena, fields, 2);

    /* Build record value */
    idl_value_field value_fields[2];

    value_fields[0].label = fields[0].label;
    value_fields[0].value = idl_value_text(&arena, "Alice", 5);

    value_fields[1].label = fields[1].label;
    value_fields[1].value = idl_value_nat32(&arena, 30);

    idl_value *record_value = idl_value_record(&arena, value_fields, 2);

    /* Add to builder and serialize */
    idl_builder_arg(&builder, record_type, record_value);

    char  *hex;
    size_t hex_len;
    if (idl_builder_serialize_hex(&builder, &hex, &hex_len) == IDL_STATUS_OK) {
        printf("Record encoded: %s\n", hex);
    }

    idl_arena_destroy(&arena);
    printf("\n");
}

/* Example 3: Decode a message */
static void example_decode(void) {
    printf("=== Example 3: Decode ===\n");

    /* Pre-encoded: ("hello", 42) */
    const uint8_t encoded[] = {
        0x44, 0x49, 0x44, 0x4c,           /* DIDL magic */
        0x00,                             /* type table count = 0 */
        0x02,                             /* arg count = 2 */
        0x71,                             /* text type */
        0x7c,                             /* int type */
        0x05,                             /* text length = 5 */
        'h',  'e',  'l',  'l',  'o', 0x2a /* int value = 42 (SLEB128) */
    };

    idl_arena arena;
    idl_arena_init(&arena, 4096);

    idl_deserializer *de = NULL;
    if (idl_deserializer_new(encoded, sizeof(encoded), &arena, &de) !=
        IDL_STATUS_OK) {
        printf("Failed to parse header\n");
        idl_arena_destroy(&arena);
        return;
    }

    printf("Decoded values:\n");
    int index = 0;
    while (!idl_deserializer_is_done(de)) {
        idl_type  *type;
        idl_value *value;
        if (idl_deserializer_get_value(de, &type, &value) != IDL_STATUS_OK) {
            printf("  Failed to decode value %d\n", index);
            break;
        }

        printf("  [%d] kind=%d", index, value->kind);
        switch (value->kind) {
        case IDL_VALUE_TEXT:
            printf(" text=\"%.*s\"", (int)value->data.text.len,
                   value->data.text.data);
            break;
        case IDL_VALUE_INT:
            printf(" int (bignum, %zu bytes)", value->data.bignum.len);
            break;
        case IDL_VALUE_NAT64:
            printf(" nat64=%lu", (unsigned long)value->data.nat64_val);
            break;
        case IDL_VALUE_BOOL:
            printf(" bool=%s", value->data.bool_val ? "true" : "false");
            break;
        default:
            break;
        }
        printf("\n");
        index++;
    }

    idl_arena_destroy(&arena);
    printf("\n");
}

/* Example 4: Roundtrip encode/decode */
static void example_roundtrip(void) {
    printf("=== Example 4: Roundtrip ===\n");

    idl_arena arena;
    idl_arena_init(&arena, 8192);

    /* Encode */
    idl_builder builder;
    idl_builder_init(&builder, &arena);
    idl_builder_arg_text(&builder, "roundtrip test", 14);
    idl_builder_arg_nat64(&builder, 12345);

    uint8_t *bytes;
    size_t   len;
    idl_builder_serialize(&builder, &bytes, &len);
    printf("Encoded %zu bytes\n", len);

    /* Decode */
    idl_deserializer *de = NULL;
    idl_deserializer_new(bytes, len, &arena, &de);

    while (!idl_deserializer_is_done(de)) {
        idl_type  *type;
        idl_value *value;
        idl_deserializer_get_value(de, &type, &value);

        if (value->kind == IDL_VALUE_TEXT) {
            printf("Decoded text: \"%.*s\"\n", (int)value->data.text.len,
                   value->data.text.data);
        } else if (value->kind == IDL_VALUE_NAT64) {
            printf("Decoded nat64: %lu\n",
                   (unsigned long)value->data.nat64_val);
        }
    }

    idl_arena_destroy(&arena);
    printf("\n");
}

int main(void) {
    example_primitives();
    example_record();
    example_decode();
    example_roundtrip();

    printf("All examples completed!\n");
    return 0;
}
