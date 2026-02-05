# LLM_C Test Layers

This document defines the test layer structure for the `llm_c` example, which has a comprehensive test suite that needs to be organized into **smoke** (quick validation) and **full** (comprehensive) layers.

## Test Structure Overview

The `llm_c` example has a rich test tree:

```
examples/llm_c/tests/
├── unit/                    # Unit tests (no PocketIC dependency)
│   ├── test_asset_manager.py
│   ├── test_chunk_utils.py
│   └── test_gguf.py
├── integration/             # Integration tests (PocketIC-based)
│   ├── test_assets.py
│   ├── test_basic.py        # ← Smoke layer candidate
│   └── test_inference.py
├── c/                       # C-level unit tests
│   ├── chunk_utils_test.c
│   └── test_assets.c
├── test_llm_c.py           # Main comprehensive test
├── test_chunk_correctness.py
└── test_stable_memory_read.py
```

## Layer Definitions

### Smoke Layer (Quick Validation)

**Purpose**: Fast, lightweight tests suitable for:
- CI quick checks
- General "examples" workflow validation
- Quick regression detection

**Target Tests**:
- `integration/test_basic.py` - Basic canister operations (hello query, minimal inference)
- **Future**: A curated subset of `test_llm_c.py` with minimal token generation

**Entry Points**:

1. **Via harness** (recommended for examples workflow):
   ```bash
   PYTHONPATH=/home/jwk/clawd/skills-local/lucid-example-test-harness/scripts \
     python -m lucid_harness --example llm_c --build --case test_basic
   ```

2. **Direct pytest** (for focused smoke runs):
   ```bash
   pytest -s examples/llm_c/tests/integration/test_basic.py
   ```

**Characteristics**:
- Runs in < 30 seconds
- Minimal resource requirements
- Validates core functionality only
- No heavy model loading or long inference chains

### Full Layer (Comprehensive Testing)

**Purpose**: Complete test coverage for:
- Focused llm_c development
- Pre-release validation
- Deep regression testing

**Target Tests**: All tests under `examples/llm_c/tests/`

**Entry Points**:

1. **Full pytest run**:
   ```bash
   pytest -s examples/llm_c/tests/
   ```

2. **Specific categories**:
   ```bash
   # Unit tests only
   pytest -s examples/llm_c/tests/unit/

   # Integration tests only
   pytest -s examples/llm_c/tests/integration/

   # Main comprehensive test
   pytest -s examples/llm_c/tests/test_llm_c.py
   ```

**Characteristics**:
- May take several minutes
- Requires model assets and significant resources
- Comprehensive coverage of all features
- Includes stress tests and edge cases

## Integration with Testing Framework

### In Examples Workflow

When running examples via harness, `llm_c` should use the **smoke layer**:

```bash
# This should run smoke tests only
python -m lucid_harness --example llm_c --build
```

The harness mapping (see `test/EXAMPLES_HARNESS_MAPPING.md`) should default to smoke layer for `llm_c`.

### In CI (Future)

**Quick CI check** (smoke layer):
```yaml
# Run smoke tests for all examples including llm_c
- name: Examples smoke tests
  run: |
    python -m lucid_harness --example llm_c --build --case test_basic
```

**Full CI check** (full layer, separate job):
```yaml
# Run full llm_c test suite (separate job, may be conditional)
- name: LLM_C full tests
  run: |
    pytest -s examples/llm_c/tests/
```

## Test Categorization

### Unit Tests (`unit/`)

- **No PocketIC dependency**
- Test pure Python/C logic
- Fast execution
- Examples:
  - `test_asset_manager.py` - Asset management logic
  - `test_chunk_utils.py` - Chunking utilities
  - `test_gguf.py` - GGUF parsing

**Run**: `pytest -s examples/llm_c/tests/unit/`

### Integration Tests (`integration/`)

- **Require PocketIC**
- Test canister behavior end-to-end
- Examples:
  - `test_basic.py` - Basic canister operations (smoke candidate)
  - `test_assets.py` - Asset upload/management
  - `test_inference.py` - Full inference workflows

**Run**: `pytest -s examples/llm_c/tests/integration/`

### C-Level Tests (`c/`)

- **Native C unit tests**
- Test low-level C implementation
- Built and run separately from Python tests

**Run**: Via CMake/ctest (part of native build)

### Top-Level Tests

- `test_llm_c.py` - Main comprehensive test (full layer)
- `test_chunk_correctness.py` - Chunking correctness validation
- `test_stable_memory_read.py` - Stable memory persistence tests

## Recommendations

### For New Tests

When adding new tests to `llm_c`:

1. **Place in appropriate directory**:
   - Unit tests → `tests/unit/`
   - Integration tests → `tests/integration/`
   - C tests → `tests/c/`

2. **Consider smoke layer**:
   - If test is fast and validates core functionality, consider adding to smoke layer
   - Smoke tests should be in `integration/test_basic.py` or a new `integration/test_smoke.py`

3. **Naming convention**:
   - Follow `test_<subject>_<behavior>.py` pattern
   - Example: `test_inference_step_chain.py`

### For Test Execution

**Quick validation** (smoke):
```bash
pytest -s examples/llm_c/tests/integration/test_basic.py
```

**Full validation** (comprehensive):
```bash
pytest -s examples/llm_c/tests/
```

**Specific category**:
```bash
pytest -s examples/llm_c/tests/integration/
pytest -s examples/llm_c/tests/unit/
```

## Future Enhancements

1. **Create dedicated smoke test file**: `tests/integration/test_smoke.py` that aggregates quick validation tests
2. **Add pytest markers**: Use `@pytest.mark.smoke` and `@pytest.mark.full` to enable selective runs
3. **Harness integration**: Enhance harness to support `--layer smoke|full` flag for llm_c
4. **CI integration**: Add separate CI jobs for smoke vs full layers

## References

- Testing framework plan: `.cursor/plans/lucid-testing-framework_bc1270c4.plan.md`
- Examples harness mapping: `test/EXAMPLES_HARNESS_MAPPING.md`
- PocketIC conventions: `test/POCKETIC_CONVENTIONS.md`
- LLM_C test README: `examples/llm_c/tests/README_TESTING.md`
