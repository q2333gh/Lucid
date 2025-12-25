# Candid Type Examples

This example demonstrates how to use various Candid data types in Internet Computer (ICP) canister interface definitions. Candid is a strongly-typed **IDL (Interface Description Language)** that supports primitive types, composite types (`record`, `variant`, `vec`, `opt`), and method signatures.

## Key Methods

### 1. Simple Query Method

```candid
"greet": (text) -> (text) query;
```

A basic query method that accepts a `text` parameter and returns a `text` value. The `query` annotation marks it as a read-only method.

### 2. Multiple Parameters

```candid
"add_user": (text, nat, bool) -> ();
```

Candid methods accept an ordered tuple of parameters. This method takes three arguments: `text`, `nat`, and `bool`, and returns nothing (`()`).

### 3. Record Type

```candid
"set_address": (text, record { street : text; city : text; zip : nat }) -> ();
"get_address": (text) -> (opt record { street : text; city : text; zip : nat }) query;
```

- `record` represents structured data with named fields
- `set_address` accepts a name and an address record
- `get_address` returns an optional address (`opt` means the value can be `null`)

### 4. Variant Type

```candid
"set_status": (text, variant { Active; Inactive; Banned : text }) -> ();
"get_status": (text) -> (variant { Active; Inactive; Banned : text }) query;
```

- `variant` represents a tagged union (enum-like type)
- Variants can be simple tags (`Active`, `Inactive`) or carry data (`Banned : text`)

### 5. Complex Nested Types

```candid
"create_profiles": (vec record { id : nat; name : text; emails : vec text; age : opt nat; ... }) -> (vec variant { Ok : ...; Err : text });
"find_profile": (nat) -> (opt record { ... }) query;
```

- Combines `record`, `vec`, `opt`, and `variant` types
- `vec` represents arrays/sequences
- `opt` represents optional values (nullable)
- Nested structures allow complex data modeling

### 6. Multiple Return Values

```candid
"stats": () -> (nat, vec nat, text) query;
```

Candid supports multiple return values as a tuple. This method returns three values: a natural number, a vector of natural numbers, and a text string.

## Key Candid Concepts

| Keyword            | Description                                       |
| ------------------ | ------------------------------------------------- |
| `record`           | Structured data with named fields (like a struct) |
| `variant`          | Tagged union (enum-like with optional data)       |
| `vec`              | Array/sequence of values                          |
| `opt`              | Optional value (can be `null`)                    |
| `service`          | Canister interface definition                     |
| `(T1, T2) -> (T3)` | Method signature: inputs `T1, T2`, output `T3`    |
| `query`            | Read-only method annotation                       |

## Implementation

See `greet.c` for C implementation examples demonstrating how to:
- Decode Candid arguments using `ic_api_from_wire_*` functions
- Encode Candid responses using `idl_builder` API
- Handle records, variants, vectors, and optional types
