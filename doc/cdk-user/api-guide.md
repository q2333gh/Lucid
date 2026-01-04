# CDK-C API Guide

Quick reference for building IC canisters in C.

## Setup

```c
#include "ic_c_sdk.h"
#include "ic_candid_builder.h"  // For simplified Candid API

IC_CANDID_EXPORT_DID()  // Auto-generate .did file
```

## Define Methods

### Query (Read-only)

```c
IC_API_QUERY(greet, "(text) -> (text)") {
    char *name = NULL;
    IC_API_PARSE_SIMPLE(api, text, name);
    
    char reply[256];
    snprintf(reply, sizeof(reply), "Hello, %s!", name);
    IC_API_REPLY_TEXT(reply);
}
```

### Update (State-changing)

```c
IC_API_UPDATE(add_user, "(text, nat, bool) -> (text)") {
    char *name = NULL;
    uint64_t age = 0;
    bool active = false;
    
    IC_API_PARSE_BEGIN(api);
    IC_ARG_TEXT(&name);
    IC_ARG_NAT(&age);
    IC_ARG_BOOL(&active);
    IC_API_PARSE_END();
    
    // Process...
    IC_API_REPLY_TEXT("User added");
}
```

## Parse Arguments

### Single Argument
```c
char *text_val = NULL;
IC_API_PARSE_SIMPLE(api, text, text_val);

uint64_t nat_val = 0;
IC_API_PARSE_SIMPLE(api, nat, nat_val);

bool bool_val = false;
IC_API_PARSE_SIMPLE(api, bool, bool_val);
```

### Multiple Arguments
```c
IC_API_PARSE_BEGIN(api);
IC_ARG_TEXT(&name);
IC_ARG_NAT(&age);
IC_ARG_BOOL(&active);
IC_API_PARSE_END();
```

## Simple Replies

```c
IC_API_REPLY_TEXT("hello");
IC_API_REPLY_NAT(42);
IC_API_REPLY_INT(-10);
IC_API_REPLY_BOOL(true);
IC_API_REPLY_BLOB(data, len);
IC_API_REPLY_EMPTY();
```

## Complex Types - Simplified API

### Record (Auto-sorted fields)

```c
IC_API_QUERY(get_address, "(text) -> (record { street : text; city : text; zip : nat })") {
    char *name = NULL;
    IC_API_PARSE_SIMPLE(api, text, name);
    
    IC_API_BUILDER_BEGIN(api) {
        // Build type (fields auto-sorted by hash)
        idl_type *addr_type = IDL_RECORD(arena,
            IDL_FIELD("street", idl_type_text(arena)),
            IDL_FIELD("city", idl_type_text(arena)),
            IDL_FIELD("zip", idl_type_nat(arena))
        );
        
        // Build value (fields auto-sorted by hash)
        idl_value *addr_val = IDL_RECORD_VALUE(arena,
            IDL_VALUE_FIELD("street", idl_value_text_cstr(arena, "Main St")),
            IDL_VALUE_FIELD("city", idl_value_text_cstr(arena, "SF")),
            IDL_VALUE_FIELD("zip", idl_value_nat32(arena, 94102))
        );
        
        idl_builder_arg(builder, addr_type, addr_val);
    }
    IC_API_BUILDER_END(api);
}
```

### Optional & Vector

```c
// Opt type
idl_type *opt_type = idl_type_opt(arena, idl_type_text(arena));
idl_value *some_val = idl_value_opt_some(arena, idl_value_text_cstr(arena, "value"));
idl_value *none_val = idl_value_opt_none(arena);

// Vec type
idl_type *vec_type = idl_type_vec(arena, idl_type_text(arena));
idl_value *items[] = {
    idl_value_text_cstr(arena, "item1"),
    idl_value_text_cstr(arena, "item2")
};
idl_value *vec_val = idl_value_vec(arena, items, 2);
```

### Variant (Auto-sorted fields)

```c
// Type
idl_type *status_type = IDL_VARIANT(arena,
    IDL_FIELD("Active", idl_type_null(arena)),
    IDL_FIELD("Inactive", idl_type_null(arena)),
    IDL_FIELD("Banned", idl_type_text(arena))
);

// Value (index must match sorted order)
idl_value_field field = {
    .label = idl_label_name("Active"),
    .value = idl_value_null(arena)
};
idl_value *status_val = idl_value_variant(arena, 1, &field);  // 1 = Active's index
```

## Builder API (Alternative)

For dynamic record construction:

```c
idl_record_builder *rb = idl_record_builder_new(arena, 3);
idl_record_builder_text(rb, "name", "Alice");
idl_record_builder_nat32(rb, "age", 30);
idl_record_builder_bool(rb, "active", true);

idl_type *type = idl_record_builder_build_type(rb);
idl_value *val = idl_record_builder_build_value(rb);
```

Available builder methods:
- `idl_record_builder_text(rb, name, value)`
- `idl_record_builder_nat32/nat64(rb, name, value)`
- `idl_record_builder_int32/int64(rb, name, value)`
- `idl_record_builder_float32/float64(rb, name, value)`
- `idl_record_builder_bool(rb, name, value)`
- `idl_record_builder_blob(rb, name, data, len)`
- `idl_record_builder_opt(rb, name, type, value)`
- `idl_record_builder_vec(rb, name, elem_type, items, count)`

## Memory Management

- `arena` auto-cleans when function exits
- No manual memory deallocation needed
- `IC_API_BUILDER_BEGIN/END` manages lifecycle

## Examples

See `examples/records/greet.c` for complete working code.
