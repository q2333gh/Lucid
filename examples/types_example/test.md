# Run all tests
pytest examples/types_example/te_test.py

# Run with build output visible (recommended to see compilation progress)
pytest -s examples/types_example/te_test.py

# Run a specific test
pytest examples/types_example/te_test.py::test_greet
pytest examples/types_example/te_test.py::test_complex_test

# Run a specific test with build output visible
pytest -s examples/types_example/te_test.py::test_greet

# Run tests matching a keyword pattern
pytest examples/types_example/te_test.py -k greet
pytest examples/types_example/te_test.py -k "greet or complex"

# Run all tests (backward compatible)
python3 examples/types_example/te_test.py

# Show more detailed output
pytest examples/types_example/te_test.py -v

# Show build output with verbose output
pytest -s -v examples/types_example/te_test.py

# Stop on the first failure
pytest examples/types_example/te_test.py -x

## Note on Build Output

By default, pytest captures stdout/stderr during test execution. To see the build
process output (compilation, optimization, etc.), use the `-s` flag:

```bash
pytest -s examples/types_example/te_test.py
```

This will show:
- `[build]` - Build process output (CMake, compilation, optimization)
- `[Setup]` - Test setup messages
- `[install]` - Canister installation messages