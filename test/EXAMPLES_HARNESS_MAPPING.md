# Examples Test Harness Mapping

This document defines how each example under `examples/*` maps to the `lucid-example-test-harness` entrypoints.

## Overview

The harness (`python -m lucid_harness`) automatically discovers test files using these patterns:
- Files matching `*_test.py` anywhere in the example directory
- Files at `test/test.py` or `<any>/test/test.py`

## Example Mappings

### Standard Examples (Auto-discovered)

These examples follow the standard naming convention and are automatically discovered by the harness:

#### `hello_lucid`
- **Test file**: `examples/hello_lucid/test/test.py`
- **Harness command**: `python -m lucid_harness --example hello_lucid --build`
- **Notes**: Standard example with PocketIC-based integration test

#### `http_request`
- **Test file**: `examples/http_request/test.py`
- **Harness command**: `python -m lucid_harness --example http_request --build`
- **Notes**: Test file is at root level (not in `test/` subdirectory)

#### `types_example`
- **Test file**: `examples/types_example/te_test.py`
- **Harness command**: `python -m lucid_harness --example types_example --build`
- **Notes**: Uses non-standard name `te_test.py` (matches `*_test.py` pattern)

### Special Cases

#### `records`
- **Test file**: `examples/records/records_test.py`
- **Harness command**: `python -m lucid_harness --example records --build`
- **Notes**: 
  - Test file is at root level
  - Generates `example_output.md` as part of test execution
  - This is a "documentation-style" test that produces verifiable output

#### `llm_c` (Multi-layer Test Suite)

The `llm_c` example has a rich test structure that requires special handling:

**Test Structure**:
- `examples/llm_c/tests/unit/` - Unit tests (no PocketIC dependency)
- `examples/llm_c/tests/integration/` - Integration tests (PocketIC-based)
- `examples/llm_c/tests/test_*.py` - Top-level test files

**Smoke Layer** (for quick validation):
- **Target**: `examples/llm_c/tests/integration/test_basic.py`
- **Harness command**: `python -m lucid_harness --example llm_c --build --case test_basic`
- **Purpose**: Quick smoke test suitable for CI and general "examples" workflow

**Full Layer** (for comprehensive testing):
- **Target**: All tests under `examples/llm_c/tests/`
- **Manual command**: `pytest -s examples/llm_c/tests/`
- **Purpose**: Complete test suite for focused llm_c development
- **Note**: This is intentionally NOT run via harness by default due to heavy resource requirements

**Harness Discovery**:
- The harness will discover all `*_test.py` files under `examples/llm_c/tests/`
- Use `--case <substring>` to filter specific tests
- Example: `python -m lucid_harness --example llm_c --build --case integration`

### Examples Without Tests

These examples currently do not have test files and will be skipped by the harness:
- `adder`
- `canister_lifecycle`
- `hooks_test`
- `ic_time_test`
- `inter-canister-call` (has test but may need special setup)
- `low_mem_warn`
- `simd128_example`
- `timer`

**Note**: When adding tests to these examples, follow the naming convention:
- Place tests in `examples/<name>/tests/test_<subject>_<behavior>.py`
- Or use `examples/<name>/test/test.py` for simple cases

## Usage Patterns

### List All Examples
```bash
PYTHONPATH=/home/jwk/clawd/skills-local/lucid-example-test-harness/scripts \
  python -m lucid_harness --list
```

### Run Standard Example Test
```bash
PYTHONPATH=/home/jwk/clawd/skills-local/lucid-example-test-harness/scripts \
  python -m lucid_harness --example hello_lucid --build
```

### Run llm_c Smoke Test
```bash
PYTHONPATH=/home/jwk/clawd/skills-local/lucid-example-test-harness/scripts \
  python -m lucid_harness --example llm_c --build --case test_basic
```

### Run Specific Test File
```bash
PYTHONPATH=/home/jwk/clawd/skills-local/lucid-example-test-harness/scripts \
  python -m lucid_harness --example types_example --build --case te_test
```

## Future Improvements

1. **Standardize test locations**: Migrate all examples to use `examples/<name>/tests/` directory
2. **Standardize naming**: Use `test_<subject>_<behavior>.py` pattern consistently
3. **Add missing tests**: Create tests for examples that currently lack them
4. **Harness enhancements**: Add support for pytest discovery and fixtures

## References

- Harness implementation: `/home/jwk/clawd/skills-local/lucid-example-test-harness/scripts/lucid_harness/cli.py`
- Test discovery patterns: See `TEST_FILE_PATTERNS` in `cli.py`
- Testing framework plan: `.cursor/plans/lucid-testing-framework_bc1270c4.plan.md`
