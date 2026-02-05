# Testing Guide

This guide explains how to run tests for the Lucid IC C SDK and your canisters.

## Overview

The testing framework provides two main entry points:

1. **Core SDK Tests** - Validate the core SDK functionality
2. **Example Tests** - Test individual example canisters

## Prerequisites

### Install Python Dependencies

```bash
pip install -r requirements.txt
```

### Install PocketIC (for integration tests)

PocketIC is required for Python integration tests that interact with canisters.

**Option 1: Use installation script** (recommended):
```bash
test/first_time_init/install_pocketic.sh
```

**Option 2: Manual setup**:
1. Download PocketIC from [GitHub releases](https://github.com/dfinity/pocketic/releases)
2. Place `pocket-ic` binary in `test/support/` or set `POCKET_IC_BIN` environment variable:
   ```bash
   export POCKET_IC_BIN=/path/to/pocket-ic
   ```

The test helpers will automatically find PocketIC if it's in:
- `test/support/pocket-ic`
- `./pocket-ic` (current directory)
- `POCKET_IC_BIN` environment variable

## Core SDK Tests

Run all core SDK tests (C unit tests + Python integration tests) with a single command:

```bash
python test/run_core_tests.py
```

This command:
1. Builds IC WASM to validate SDK compiles for IC platform
2. Builds and runs native C tests (via `python build.py --test`)
3. Runs Python PocketIC integration tests (`test/inter-canister-call/`, `test/basic_test/`)

### What Gets Tested

- **C Unit Tests**: Candid encoding/decoding, IC API bindings, memory management
- **Python Integration Tests**: Inter-canister calls, basic canister operations

## Example Tests

### Using the Test Harness (Recommended)

The test harness provides a unified way to build and test examples:

**List available examples**:
```bash
PYTHONPATH=/home/jwk/clawd/skills-local/lucid-example-test-harness/scripts \
  python -m lucid_harness --list
```

**Run a specific example**:
```bash
PYTHONPATH=/home/jwk/clawd/skills-local/lucid-example-test-harness/scripts \
  python -m lucid_harness --example hello_lucid --build
```

**Run with specific test filter**:
```bash
PYTHONPATH=/home/jwk/clawd/skills-local/lucid-example-test-harness/scripts \
  python -m lucid_harness --example types_example --build --case te_test
```

### Direct pytest (Alternative)

You can also run example tests directly with pytest:

```bash
# Run specific example test
pytest -s examples/types_example/te_test.py

# Run specific test function
pytest examples/types_example/te_test.py::test_greet
```

### LLM_C Example (Special Case)

The `llm_c` example has a comprehensive test suite with two layers:

**Smoke tests** (quick validation):
```bash
pytest -s examples/llm_c/tests/integration/test_basic.py
```

**Full test suite** (comprehensive, takes longer):
```bash
pytest -s examples/llm_c/tests/
```

See `examples/llm_c/tests/TEST_LAYERS.md` for details.

## Writing Tests

### Basic Test Structure

```python
import pytest
from test.support.test_support_pocketic import install_example_canister
from ic.candid import encode, decode, Types
from ic.principal import Principal

@pytest.fixture
def canister():
    """Install canister once per test."""
    pic, canister_id = install_example_canister("example_name", auto_build=False)
    principal = Principal.from_str(canister_id) if isinstance(canister_id, str) else canister_id
    return pic, principal

def test_my_feature(canister):
    pic, principal = canister
    
    # Encode arguments with explicit types
    args = [{"type": Types.Text, "value": "hello"}]
    result = pic.query_call(principal, "method_name", encode(args))
    
    # Decode and verify
    decoded = decode(result)
    assert decoded[0]['value'] == "expected"
```

### Key Conventions

1. **Use shared helpers**: Always use `install_example_canister` or `install_multiple_examples` from `test/support/test_support_pocketic.py`
2. **Explicit Candid types**: Always specify `Types.*` when encoding arguments
3. **Use pytest fixtures**: Share canister setup across tests
4. **Query vs Update**: Use `query_call` for read-only operations, `update_call` for state changes

### Candid Type Examples

**Primitive types**:
```python
{"type": Types.Text, "value": "hello"}
{"type": Types.Nat, "value": 42}
{"type": Types.Bool, "value": True}
```

**Complex types**:
```python
# Record
{"type": Types.Record({"name": Types.Text, "age": Types.Nat}),
 "value": {"name": "Alice", "age": 30}}

# Variant
{"type": Types.Variant({"Ok": Types.Text, "Err": Types.Text}),
 "value": ("Ok", "success")}

# Vec
{"type": Types.Vec(Types.Text), "value": ["a", "b", "c"]}

# Opt
{"type": Types.Opt(Types.Text), "value": [("some", "value")]}  # Some
{"type": Types.Opt(Types.Text), "value": []}                   # None
```

## Best Practices

1. **Use fixtures** - Share canister instance across tests to avoid repeated builds
2. **Test isolation** - Each test should be independent and not rely on state from other tests
3. **Clear assertions** - Use descriptive error messages in assertions
4. **View output** - Use `-s` flag with pytest to see build logs and debug output
5. **Skip builds when possible** - Use `auto_build=False` for repeated test runs

## Troubleshooting

### PocketIC Not Found

If you see "pocket-ic binary not found":
1. Run `test/first_time_init/install_pocketic.sh`
2. Or set `POCKET_IC_BIN` environment variable
3. Or place `pocket-ic` binary in `test/support/` directory

### Build Failures

If tests fail during build:
1. Check that `python build.py --icwasm` works independently
2. Ensure WASI SDK is properly configured (see `doc/cdk-user/build-guide.md`)
3. Check build logs in test output

### Import Errors

If you see import errors:
1. Ensure you're running from repository root
2. Check that `requirements.txt` dependencies are installed
3. Verify Python path includes repository root

## References

- **PocketIC Conventions**: `test/POCKETIC_CONVENTIONS.md` - Detailed testing patterns and conventions
- **Examples Harness Mapping**: `test/EXAMPLES_HARNESS_MAPPING.md` - How examples map to harness
- **LLM_C Test Layers**: `examples/llm_c/tests/TEST_LAYERS.md` - Smoke vs full test layers
- **Developer Testing Guide**: `doc/cdk-dev/test.md` - Advanced testing topics for contributors
