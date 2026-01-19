# Minimal Candid .did Guide (User-Facing)

This is a tiny, user-focused cheat sheet for writing Candid interface files.
It does not cover the full spec. It is enough for common canister interfaces.

## File Shape

A `.did` file usually declares a `service` with methods:

```
service : {
  greet: (text) -> (text);
  add: (nat, nat) -> (nat);
}
```

Each method is:

```
name: (arg_types) -> (return_types) [annotations];
```

Annotations (optional):
- `query` (read-only)
- `oneway` (no response)
- `composite_query`

Example:

```
service : {
  get: () -> (text) query;
  notify: (text) -> () oneway;
}
```

## Primitive Types

```
null, bool,
nat, int,
nat8, nat16, nat32, nat64,
int8, int16, int32, int64,
float32, float64,
text,
principal
```

## Common Containers

```
opt T        // optional value
vec T        // vector/list
record { ... }
variant { ... }
```

Examples:

```
type User = record {
  id: nat64;
  name: text;
  email: opt text;
};

type Status = variant {
  ok;
  not_found;
  error: text;
};

service : {
  create: (User) -> (Status);
  list: () -> (vec User);
}
```

## Records

Records are named fields. Order does not matter in textual `.did`.

```
record {
  id: nat;
  name: text;
}
```

## Variants

Variants are tagged unions. Use labels with or without payloads.

```
variant {
  ok: nat;
  error: text;
  empty;
}
```

## Type Aliases

You can alias types to keep interfaces readable.

```
type UserId = nat64;
type Result = variant { ok: text; err: text };
```

## Full Example

```
type User = record {
  id: nat64;
  name: text;
  email: opt text;
};

type Result = variant { ok: User; err: text };

service : {
  get_user: (nat64) -> (Result) query;
  set_user: (User) -> ();
}
```

## Notes

- `record` and `variant` field labels must be unique.
- `principal` represents a canister or user identity.
- Keep types stable over time to avoid breaking clients.
