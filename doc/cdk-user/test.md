# Testing Guide

Automated testing with pytest + PocketIC.

## Quick Start

```bash
# Install dependencies
pip install -r requirements.txt

# Run tests (auto-builds canister)
pytest -s examples/types_example/te_test.py

# Run specific test
pytest examples/types_example/te_test.py::test_greet
```

## Test Structure

```python
import pytest
from ic.candid import Types, encode
from ic.principal import Principal
from test.support.test_support_pocketic import (
    decode_candid_text,
    install_example_canister,
)

@pytest.fixture(scope="module")
def pic_and_canister():
    """Build and install canister once for all tests"""
    pic, canister_id = install_example_canister("types_example", auto_build=True)
    return pic, canister_id

@pytest.fixture
def pic(pic_and_canister):
    return pic_and_canister[0]

@pytest.fixture
def canister_id(pic_and_canister):
    return pic_and_canister[1]

def test_greet(pic, canister_id):
    """Test query method"""
    params = [{"type": Types.Text, "value": "Alice"}]
    payload = encode(params)
    principal_id = Principal.from_str(str(canister_id))
    response_bytes = pic.query_call(principal_id, "greet", payload)
    decoded = decode_candid_text(response_bytes)
    assert decoded == "Hello, Alice! Welcome to IC C SDK."
```

## Candid Types

### Basic Types
```python
# Text
[{"type": Types.Text, "value": "hello"}]

# Nat/Int/Bool
[{"type": Types.Nat, "value": 42}]
[{"type": Types.Int, "value": -10}]
[{"type": Types.Bool, "value": True}]

# Multiple params
[
    {"type": Types.Text, "value": "Alice"},
    {"type": Types.Nat, "value": 30},
    {"type": Types.Bool, "value": True}
]
```

### Complex Types
```python
# Record
[{
    "type": Types.Record({"name": Types.Text, "age": Types.Nat}),
    "value": {"name": "Alice", "age": 30}
}]

# Variant
[{
    "type": Types.Variant({"Active": Types.Null, "Banned": Types.Text}),
    "value": {"Active": None}
}]

# Vec
[{"type": Types.Vec(Types.Text), "value": ["a", "b", "c"]}]

# Opt
[{"type": Types.Opt(Types.Text), "value": ["some"]}]  # Some
[{"type": Types.Opt(Types.Text), "value": []}]        # None
```

## Query vs Update

```python
# Query (read-only)
response = pic.query_call(principal_id, "method_name", payload)

# Update (state-changing)
response = pic.update_call(principal_id, "method_name", payload)
```

## Best Practices

1. **Use fixtures** - Share canister instance across tests
2. **Test isolation** - Each test should be independent
3. **Clear assertions** - Use descriptive error messages
4. **View output** - Use `-s` flag to see build logs

## Examples

See `examples/types_example/te_test.py` for complete examples.
