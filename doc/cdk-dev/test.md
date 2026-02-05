# Developer Testing Guide

This guide covers advanced testing topics for contributors and developers working on the Lucid IC C SDK.

## Test Framework Architecture

The testing framework is organized into several layers:

### Test Directory Structure

```
test/
├── core_c/              # (Future) C unit tests (c_candid, etc.)
├── core_py/             # (Future) Python core tests (inter-canister-call, basic_test)
├── support/             # Shared helpers and infrastructure
│   ├── test_support_build.py
│   └── test_support_pocketic.py
├── run_core_tests.py    # Core SDK test entrypoint
├── EXAMPLES_HARNESS_MAPPING.md
└── POCKETIC_CONVENTIONS.md

examples/
├── <name>/
│   └── tests/          # (Target) Standard location for example tests
│       └── test_<subject>_<behavior>.py
└── llm_c/
    └── tests/
        ├── unit/       # Unit tests (no PocketIC)
        ├── integration/ # Integration tests (PocketIC-based)
        └── TEST_LAYERS.md
```

### Test Types

1. **C Unit Tests**: Low-level Candid, IC API, memory behavior
   - Location: `test/c_candid/`, `cdk-c/test/`
   - Build: Via CMake/ctest (native build)
   - Run: `python build.py --test` or `ctest`

2. **Python PocketIC Integration Tests**: Canister behavior testing
   - Location: `test/inter-canister-call/`, `test/basic_test/`, `examples/*/tests/`
   - Build: Via `build.py --icwasm --examples <name>`
   - Run: `pytest` or harness

3. **Example Scenario Tests**: Per-example acceptance tests
   - Location: `examples/*/tests/`
   - Run: Via harness (`python -m lucid_harness`)

## Core Test Entrypoint

The core test script (`test/run_core_tests.py`) orchestrates:

1. **IC WASM Build**: `python build.py --icwasm`
   - Validates SDK compiles for IC platform
   - Ensures WASM post-processing works

2. **Native C Tests**: `python build.py --test`
   - Builds native version
   - Runs CTest-based tests (cdk-c/test, test/c_candid)

3. **Python Integration Tests**: `pytest test/inter-canister-call test/basic_test`
   - PocketIC-based integration tests
   - Validates core SDK behavior

## Examples Harness

The `lucid-example-test-harness` provides unified example testing:

- **Discovery**: Automatically finds test files matching `*_test.py` or `test/test.py`
- **Build integration**: Optionally builds WASM before testing (`--build`)
- **Artifact generation**: Produces logs and summaries for CI/docs

See `test/EXAMPLES_HARNESS_MAPPING.md` for detailed mappings.

## Test Naming Conventions

### Target Structure (for new tests)

**Core tests**:
- `test/core_c/` - C unit tests
- `test/core_py/` - Python core tests

**Example tests**:
- `examples/<name>/tests/test_<subject>_<behavior>.py`
- Example: `test_hello_basic_candid.py`, `test_http_request_happy_path.py`

**Migration strategy**:
- New tests: Must follow target structure
- Existing tests: Migrate incrementally when touched

## PocketIC Testing Conventions

All new Python tests should follow conventions in `test/POCKETIC_CONVENTIONS.md`:

- Use `install_example_canister` / `install_multiple_examples`
- Explicit `Types.*` for all Candid encoding
- Pytest fixtures for setup
- Clear separation of setup, execution, assertions

## LLM_C Test Layers

The `llm_c` example uses a two-layer approach:

- **Smoke layer**: Quick validation (`integration/test_basic.py`)
- **Full layer**: Comprehensive suite (all tests)

See `examples/llm_c/tests/TEST_LAYERS.md` for details.

## Adding New Tests

### For Core SDK

1. **C tests**: Add to `test/c_candid/` or `cdk-c/test/`
2. **Python tests**: Add to `test/core_py/` (future) or appropriate subdirectory
3. **Update entrypoint**: Ensure `test/run_core_tests.py` includes new tests

### For Examples

1. **Create test file**: `examples/<name>/tests/test_<subject>_<behavior>.py`
2. **Follow conventions**: Use PocketIC helpers and pytest fixtures
3. **Harness discovery**: Tests matching `*_test.py` are auto-discovered

### Test File Template

```python
#!/usr/bin/env python3
"""
Test description.
"""
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

def test_feature_name(canister):
    """Test description."""
    pic, principal = canister
    
    # Setup
    args = [{"type": Types.Text, "value": "input"}]
    
    # Execute
    result = pic.query_call(principal, "method_name", encode(args))
    decoded = decode(result)
    
    # Assert
    assert decoded[0]['value'] == "expected"
```

## CI Integration (Future)

The test framework is designed to support CI:

- **Core tests**: `python test/run_core_tests.py`
- **Examples smoke**: `python -m lucid_harness --example <name> --build`
- **LLM_C smoke**: `pytest examples/llm_c/tests/integration/test_basic.py`

Full test suites (like llm_c full layer) should run in separate CI jobs due to resource requirements.

## Debugging Tests

### View Build Output

```bash
# pytest with output
pytest -s test/inter-canister-call/

# harness with verbose output
python -m lucid_harness --example hello_lucid --build
```

### Check Artifacts

Harness produces artifacts in:
- `artifacts/<example>/<timestamp>/build.*.txt`
- `artifacts/<example>/<timestamp>/*.stdout.txt`
- `artifacts/<example>/<timestamp>/summary.json`

### Common Issues

1. **PocketIC not found**: Check `POCKET_IC_BIN` or install via script
2. **Build failures**: Run `python build.py --icwasm` independently first
3. **Import errors**: Ensure running from repo root, check Python path

## References

- **Testing Framework Plan**: `.cursor/plans/lucid-testing-framework_bc1270c4.plan.md`
- **PocketIC Conventions**: `test/POCKETIC_CONVENTIONS.md`
- **Examples Mapping**: `test/EXAMPLES_HARNESS_MAPPING.md`
- **LLM_C Layers**: `examples/llm_c/tests/TEST_LAYERS.md`
- **User Testing Guide**: `doc/cdk-user/test.md`
