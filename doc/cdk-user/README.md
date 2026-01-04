# CDK-C User Documentation

Documentation for developing IC canisters in C.

## 📚 Guides

- **[API Guide](api-guide.md)** - Write canister code with simplified API
- **[Build Guide](build-guide.md)** - Build and optimize WASM
- **[Test Guide](test.md)** - Test with pytest + PocketIC

## Quick Start

```bash
# 1. Build canister
python build.py --icwasm --examples types_example

# 2. Run tests
pytest -s examples/types_example/te_test.py
```

## Examples

- `examples/types_example/` - Complete working examples
- `examples/records/` - Simplified API examples
