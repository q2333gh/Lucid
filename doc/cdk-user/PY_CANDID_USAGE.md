# Candid Type Encoding Guide

## Overview

This guide demonstrates how to encode complex Candid types using `ic-py` library for testing Internet Computer canisters.

**Library**: `ic-py` (https://github.com/rocklabs-io/ic-py)  
**Installation**: `pip3 install ic-py`

## Complex Type Support

`ic-py` fully supports complex nested types:
- `Record` (structured data)
- `Variant` (tagged unions)
- `Vec` (arrays)
- `Opt` (optional values)
- Arbitrary nesting combinations

## Encoding Examples

### Record Type

```python
from ic.candid import Types, encode

# Define Record type
address_type = Types.Record({
    "street": Types.Text,
    "city": Types.Text,
    "zip": Types.Nat
})

# Encode parameters
params = [
    {"type": Types.Text, "value": "Bob"},
    {
        "type": address_type,
        "value": {
            "street": "123 Main St",
            "city": "San Francisco",
            "zip": 94102
        }
    }
]
payload = encode(params)
```

### Variant Type

```python
# Define Variant type
status_type = Types.Variant({
    "Active": Types.Null,
    "Inactive": Types.Null,
    "Banned": Types.Text
})

# Encode parameters
params = [
    {"type": Types.Text, "value": "user1"},
    {
        "type": status_type,
        "value": {"Active": None}  # Select Active variant
    }
]

# Or with data
params2 = [
    {"type": Types.Text, "value": "user2"},
    {
        "type": status_type,
        "value": {"Banned": "spam detected"}
    }
]
payload = encode(params)
```

### Nested Types

```python
# Complex nested type: vec record with opt and variant
status_type = Types.Variant({
    "Active": Types.Null,
    "Inactive": Types.Null,
    "Banned": Types.Text
})

profile_type = Types.Record({
    "id": Types.Nat,
    "name": Types.Text,
    "emails": Types.Vec(Types.Text),
    "age": Types.Opt(Types.Nat),
    "status": status_type
})

params = [{
    "type": Types.Vec(profile_type),
    "value": [{
        "id": 1,
        "name": "Alice",
        "emails": ["alice@example.com"],
        "age": [30],  # Opt: [value] = Some(value), [] = None
        "status": {"Active": None}
    }]
}]
payload = encode(params)
```

## PocketIC DID File Binding

**Library**: `pocket-ic` (https://github.com/dfinity/pocketic)

PocketIC's `create_and_install_canister_with_candid` method supports DID file binding:

```python
from pocket_ic import PocketIC

pic = PocketIC()

# Read DID file
with open("types_example.did", "r") as f:
    candid_interface = f.read()

# Read WASM file
with open("types_example_ic.wasm", "rb") as f:
    wasm_module = f.read()

# Create and install canister with DID binding
canister = pic.create_and_install_canister_with_candid(
    candid=candid_interface,  # DID file content (string)
    wasm_module=wasm_module,   # WASM module (bytes)
    init_args=None            # Optional init arguments
)

# Use canister object for type-safe method calls
result = canister.greet("World")
```

## Key Points

1. **Type Definition**: Use `Types.Record({...})` and `Types.Variant({...})` to define complex types
2. **Encoding Format**: Use `{"type": type_obj, "value": value_dict}` format
3. **Opt Type**: Use list notation - `[value]` = Some(value), `[]` = None
4. **Variant Value**: Use dict `{"VariantName": value}` or `{"VariantName": None}`
5. **DID Binding**: PocketIC's `create_and_install_canister_with_candid` handles DID binding automatically

## References

- ic-py GitHub: https://github.com/rocklabs-io/ic-py
- PocketIC GitHub: https://github.com/dfinity/pocketic
