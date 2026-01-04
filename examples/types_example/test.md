# Run all tests
pytest examples/types_example/te_test.py

# Run a specific test
pytest examples/types_example/te_test.py::test_greet
pytest examples/types_example/te_test.py::test_complex_test

# Run tests matching a keyword pattern
pytest examples/types_example/te_test.py -k greet
pytest examples/types_example/te_test.py -k "greet or complex"

# Run all tests (backward compatible)
python3 examples/types_example/te_test.py

# Show more detailed output
pytest examples/types_example/te_test.py -v

# Stop on the first failure
pytest examples/types_example/te_test.py -x