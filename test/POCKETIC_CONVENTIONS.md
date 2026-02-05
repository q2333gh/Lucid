# PocketIC Testing Conventions

This document codifies the standard patterns and conventions for writing PocketIC-based integration tests in the Lucid IC C SDK project.

## Core Principles

All new Python tests that interact with IC canisters should follow these conventions:

1. **Use shared helpers** from `test/support/test_support_pocketic.py`
2. **Explicit Candid types** - always use `Types.*` when encoding/decoding
3. **Pytest fixtures** - prefer fixtures for canister setup and reuse
4. **Clear test structure** - separate setup, execution, and assertions

## Installation Patterns

### Single Canister Installation

**Standard pattern**:
```python
from test.support.test_support_pocketic import install_example_canister
from ic.candid import encode, decode, Types
from ic.principal import Principal

def test_my_feature():
    # Install canister (auto-builds if needed)
    pic, canister_id = install_example_canister("example_name", auto_build=True)
    
    # Convert to Principal for calls
    principal = Principal.from_str(canister_id) if isinstance(canister_id, str) else canister_id
    
    # Make calls...
```

**With manual build control**:
```python
# Skip build if already built (faster for repeated runs)
pic, canister_id = install_example_canister("example_name", auto_build=False)
```

### Multiple Canister Installation

**For inter-canister tests**:
```python
from test.support.test_support_pocketic import install_multiple_examples

def test_inter_canister():
    # Install multiple canisters on same PocketIC instance
    pic, canister_ids = install_multiple_examples(
        ["adder", "inter-canister-call"],
        auto_build=True
    )
    
    adder_id = canister_ids["adder"]
    caller_id = canister_ids["inter-canister-call"]
    
    # Use principals directly (already Principal type)
    result = pic.update_call(caller_id, "trigger_call", payload)
```

## Candid Encoding/Decoding

### Always Use Explicit Types

**✅ Good** - Explicit type specification:
```python
from ic.candid import encode, decode, Types

# Single argument
args = [{"type": Types.Text, "value": "hello"}]
result = pic.update_call(principal, "method_name", encode(args))

# Multiple arguments
args = [
    {"type": Types.Text, "value": "name"},
    {"type": Types.Nat, "value": 42},
    {"type": Types.Bool, "value": True}
]
result = pic.update_call(principal, "add_user", encode(args))

# Decode result
decoded = decode(result)
value = decoded[0]['value']  # Extract first return value
```

**❌ Bad** - Implicit or missing types:
```python
# Don't rely on type inference
args = ["hello"]  # Missing type specification
result = pic.update_call(principal, "method", encode(args))  # May fail
```

### Common Type Patterns

**Primitive types**:
```python
{"type": Types.Text, "value": "string"}
{"type": Types.Nat, "value": 42}
{"type": Types.Nat32, "value": 100}
{"type": Types.Bool, "value": True}
{"type": Types.Principal, "value": Principal.from_str("aaaaa-aa")}
```

**Collections**:
```python
# Vec (list/array)
{"type": Types.Vec(Types.Nat8), "value": [1, 2, 3, 4]}
{"type": Types.Vec(Types.Text), "value": ["a", "b", "c"]}

# Opt (optional)
{"type": Types.Opt(Types.Text), "value": [("some", "value")]}  # Some
{"type": Types.Opt(Types.Text), "value": []}                   # None

# Blob (byte array)
{"type": Types.Vec(Types.Nat8), "value": list(b"data")}
```

**Complex types**:
```python
# Record
args = [{
    "type": Types.Record({
        "id": Types.Nat,
        "name": Types.Text,
        "active": Types.Bool
    }),
    "value": {
        "id": 1,
        "name": "Alice",
        "active": True
    }
}]

# Variant (tagged union)
args = [{
    "type": Types.Variant({
        "Ok": Types.Text,
        "Err": Types.Text
    }),
    "value": ("Ok", "success message")  # (tag, value) tuple
}]
```

## Pytest Fixtures

### Basic Fixture Pattern

**Recommended structure**:
```python
import pytest
from test.support.test_support_pocketic import install_example_canister
from ic.candid import encode, decode, Types
from ic.principal import Principal

@pytest.fixture
def canister():
    """Fixture to install canister once per test."""
    pic, canister_id = install_example_canister("example_name", auto_build=False)
    principal = Principal.from_str(canister_id) if isinstance(canister_id, str) else canister_id
    return pic, principal

def test_with_fixture(canister):
    pic, principal = canister
    args = [{"type": Types.Text, "value": "test"}]
    result = pic.query_call(principal, "method", encode(args))
    decoded = decode(result)
    assert decoded[0]['value'] == "expected"
```

### Shared Fixture for Multiple Tests

**For test suites**:
```python
@pytest.fixture(scope="module")  # Shared across all tests in module
def canister_setup():
    """Install canister once for entire test module."""
    pic, canister_id = install_example_canister("example_name", auto_build=True)
    principal = Principal.from_str(canister_id) if isinstance(canister_id, str) else canister_id
    yield pic, principal
    # Cleanup if needed (PocketIC instance is automatically cleaned up)

def test_feature_a(canister_setup):
    pic, principal = canister_setup
    # Test A...

def test_feature_b(canister_setup):
    pic, principal = canister_setup
    # Test B...
```

## Method Call Patterns

### Update Calls (State-changing)

```python
# Update calls modify canister state
args = [{"type": Types.Text, "value": "data"}]
result = pic.update_call(principal, "update_method", encode(args))
decoded = decode(result)
```

### Query Calls (Read-only)

```python
# Query calls are read-only (faster, no state change)
args = [{"type": Types.Nat, "value": 42}]
result = pic.query_call(principal, "query_method", encode(args))
decoded = decode(result)
```

### No Arguments

```python
# Empty args list for methods with no parameters
result = pic.update_call(principal, "reset", encode([]))
```

## Error Handling

### Handling Rejections

```python
from ic.candid import decode, Types

try:
    args = [{"type": Types.Nat, "value": 999999}]
    result = pic.update_call(principal, "get_user", encode(args))
    decoded = decode(result)
    
    # Check for error variant
    variant = decoded[0]['value']  # tuple (tag, value)
    tag, value = variant
    if tag == "Err":
        assert "not found" in value.lower()
except Exception as e:
    # Or catch PocketIC rejection
    assert "not found" in str(e).lower()
```

## Best Practices

### 1. Use `auto_build=False` for Repeated Runs

When running tests repeatedly, skip the build step:
```python
pic, canister_id = install_example_canister("example", auto_build=False)
```

### 2. Always Convert canister_id to Principal

```python
# Check type before conversion
if isinstance(canister_id, str):
    principal = Principal.from_str(canister_id)
else:
    principal = canister_id  # Already Principal
```

### 3. Use Query Calls When Possible

Prefer `query_call` for read-only operations (faster, no state change):
```python
# ✅ Good - read-only
result = pic.query_call(principal, "get_value", encode([]))

# ❌ Avoid - unnecessary update call
result = pic.update_call(principal, "get_value", encode([]))
```

### 4. Decode Results Before Assertions

Always decode Candid responses before checking values:
```python
result = pic.query_call(principal, "method", encode(args))
decoded = decode(result)
assert decoded[0]['value'] == "expected"  # ✅ Good

# ❌ Bad - checking raw bytes
assert result == b"expected"
```

### 5. Handle Variant Results Properly

```python
decoded = decode(result)
variant = decoded[0]['value']  # tuple (tag, value)
tag, value = variant

if tag == "Ok":
    print(f"Success: {value}")
elif tag == "Err":
    print(f"Error: {value}")
```

## Environment Setup

### PocketIC Binary Location

The helpers automatically search for `pocket-ic` binary in this order:
1. `test/support/pocket-ic` (next to test helpers)
2. `./pocket-ic` (current working directory)
3. `POCKET_IC_BIN` environment variable

**Manual setup**:
```bash
export POCKET_IC_BIN=/path/to/pocket-ic
```

Or use the installation script:
```bash
test/first_time_init/install_pocketic.sh
```

## Example: Complete Test

```python
#!/usr/bin/env python3
"""
Example test following PocketIC conventions.
"""
import pytest
from test.support.test_support_pocketic import install_example_canister
from ic.candid import encode, decode, Types
from ic.principal import Principal

@pytest.fixture
def canister():
    """Install canister once per test."""
    pic, canister_id = install_example_canister("hello_lucid", auto_build=False)
    principal = Principal.from_str(canister_id) if isinstance(canister_id, str) else canister_id
    return pic, principal

def test_greet_no_arg(canister):
    """Test query method with no arguments."""
    pic, principal = canister
    
    # Query call (read-only)
    result = pic.query_call(principal, "greet_no_arg", encode([]))
    decoded = decode(result)
    
    # Assert on decoded value
    assert decoded[0]['value'] == "hello world from cdk-c !"

def test_greet_with_name(canister):
    """Test query method with text argument."""
    pic, principal = canister
    
    # Encode with explicit type
    args = [{"type": Types.Text, "value": "Alice"}]
    result = pic.query_call(principal, "greet", encode(args))
    decoded = decode(result)
    
    assert "Alice" in decoded[0]['value']
```

## References

- PocketIC testing skill: `.cursor/skills/pocketic-testing/SKILL.md`
- Helper implementation: `test/support/test_support_pocketic.py`
- Build helpers: `test/support/test_support_build.py`
- Testing framework plan: `.cursor/plans/lucid-testing-framework_bc1270c4.plan.md`
