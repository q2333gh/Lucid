# C Candid Runtime

A pure C implementation of the [Candid](https://github.com/dfinity/candid) serialization format for the Internet Computer.

## Quick Start

### Include the main header

```c
#include "idl/runtime.h"
```

This single header includes everything you need:
- Arena allocator (`idl_arena`)
- Type system (`idl_type`, `idl_type_*`)
- Value representation (`idl_value`, `idl_value_*`)
- Serialization (`idl_builder`)
- Deserialization (`idl_deserializer`)
- Subtyping and coercion

### Build

```bash
cd c_candid
mkdir -p build && cd build
cmake ..
make
```

This produces:
- `lib/libcandid_runtime.a` - Static library
- `bin/c_candid_encode` - CLI encoder
- `bin/c_candid_decode` - CLI decoder
- `bin/basic_usage` - Example program

### Link against the library

```bash
clang -I/path/to/c_candid/runtime/runtime/include \
      your_code.c \
      -L/path/to/c_candid/build/lib -lcandid_runtime \
      -o your_program
```

## Usage Examples

### 1. Encode primitives

```c
#include "idl/runtime.h"

int main(void) {
    // Initialize arena allocator
    idl_arena arena;
    idl_arena_init(&arena, 4096);

    // Create builder
    idl_builder builder;
    idl_builder_init(&builder, &arena);

    // Add arguments
    idl_builder_arg_text(&builder, "hello", 5);
    idl_builder_arg_nat64(&builder, 42);
    idl_builder_arg_bool(&builder, 1);

    // Serialize to binary
    uint8_t *bytes;
    size_t len;
    idl_builder_serialize(&builder, &bytes, &len);

    // Or get hex string
    char *hex;
    size_t hex_len;
    idl_builder_serialize_hex(&builder, &hex, &hex_len);
    printf("Hex: %s\n", hex);

    // Cleanup
    idl_arena_destroy(&arena);
    return 0;
}
```

### 2. Decode a message

```c
#include "idl/runtime.h"

int main(void) {
    // Binary Candid data (e.g., from network)
    const uint8_t data[] = { 0x44, 0x49, 0x44, 0x4c, ... };
    size_t data_len = sizeof(data);

    idl_arena arena;
    idl_arena_init(&arena, 4096);

    // Create deserializer
    idl_deserializer *de = NULL;
    if (idl_deserializer_new(data, data_len, &arena, &de) != IDL_STATUS_OK) {
        fprintf(stderr, "Failed to parse\n");
        return 1;
    }

    // Read values
    while (!idl_deserializer_is_done(de)) {
        idl_type *type;
        idl_value *value;
        idl_deserializer_get_value(de, &type, &value);

        switch (value->kind) {
        case IDL_VALUE_TEXT:
            printf("text: %.*s\n", (int)value->data.text.len, 
                   value->data.text.data);
            break;
        case IDL_VALUE_NAT64:
            printf("nat64: %lu\n", value->data.nat64_val);
            break;
        // ... handle other types
        }
    }

    idl_arena_destroy(&arena);
    return 0;
}
```

### 3. Build complex types (records, variants)

```c
// Create record type: { name: text; age: nat32 }
idl_field fields[2];

fields[0].label.kind = IDL_LABEL_NAME;
fields[0].label.id = idl_hash("name");
fields[0].label.name = "name";
fields[0].type = idl_type_text(&arena);

fields[1].label.kind = IDL_LABEL_NAME;
fields[1].label.id = idl_hash("age");
fields[1].label.name = "age";
fields[1].type = idl_type_nat32(&arena);

idl_type *record_type = idl_type_record(&arena, fields, 2);

// Create record value
idl_value_field value_fields[2];
value_fields[0].label = fields[0].label;
value_fields[0].value = idl_value_text(&arena, "Alice", 5);
value_fields[1].label = fields[1].label;
value_fields[1].value = idl_value_nat32(&arena, 30);

idl_value *record = idl_value_record(&arena, value_fields, 2);

// Add to builder
idl_builder_arg(&builder, record_type, record);
```

## API Reference

### Core Types

| Type | Description |
|------|-------------|
| `idl_arena` | Arena allocator for memory management |
| `idl_type` | Candid type representation |
| `idl_value` | Candid value representation |
| `idl_builder` | Message builder for serialization |
| `idl_deserializer` | Message parser for deserialization |

### Primitive Type Constructors

```c
idl_type *idl_type_null(idl_arena *arena);
idl_type *idl_type_bool(idl_arena *arena);
idl_type *idl_type_nat(idl_arena *arena);
idl_type *idl_type_nat8(idl_arena *arena);
idl_type *idl_type_nat16(idl_arena *arena);
idl_type *idl_type_nat32(idl_arena *arena);
idl_type *idl_type_nat64(idl_arena *arena);
idl_type *idl_type_int(idl_arena *arena);
idl_type *idl_type_int8(idl_arena *arena);
idl_type *idl_type_int16(idl_arena *arena);
idl_type *idl_type_int32(idl_arena *arena);
idl_type *idl_type_int64(idl_arena *arena);
idl_type *idl_type_float32(idl_arena *arena);
idl_type *idl_type_float64(idl_arena *arena);
idl_type *idl_type_text(idl_arena *arena);
idl_type *idl_type_principal(idl_arena *arena);
```

### Composite Type Constructors

```c
idl_type *idl_type_opt(idl_arena *arena, idl_type *inner);
idl_type *idl_type_vec(idl_arena *arena, idl_type *inner);
idl_type *idl_type_record(idl_arena *arena, idl_field *fields, size_t len);
idl_type *idl_type_variant(idl_arena *arena, idl_field *fields, size_t len);
```

### Builder Convenience Functions

```c
idl_status idl_builder_arg_null(idl_builder *builder);
idl_status idl_builder_arg_bool(idl_builder *builder, int v);
idl_status idl_builder_arg_nat64(idl_builder *builder, uint64_t v);
idl_status idl_builder_arg_int64(idl_builder *builder, int64_t v);
idl_status idl_builder_arg_float64(idl_builder *builder, double v);
idl_status idl_builder_arg_text(idl_builder *builder, const char *s, size_t len);
idl_status idl_builder_arg_blob(idl_builder *builder, const uint8_t *data, size_t len);
// ... and more
```

## Testing

```bash
# Run candid unit tests
./build/bin/runtime_tests

# Run oracle tests (compares with Rust didc)
python3 oracle/smoke.py
```

## Project Structure

```
c_candid/
├── runtime/
│   ├── runtime/
│   │   ├── include/idl/    # Public headers
│   │   │   ├── runtime.h   # Main include file
│   │   │   ├── arena.h
│   │   │   ├── types.h
│   │   │   ├── value.h
│   │   │   ├── builder.h
│   │   │   ├── deserializer.h
│   │   │   └── ...
│   │   └── src/            # Implementation
│   ├── src/                # CLI tools
│   └── tests/              # Unit tests
├── oracle/                 # Python oracle scripts
├── examples/               # Usage examples
└── CMakeLists.txt
```

## License

Same license as the parent candid repository.

