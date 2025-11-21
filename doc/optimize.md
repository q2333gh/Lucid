# WASM Optimization

## Automatic Optimization

Build script automatically optimizes WASM files using `wasm-opt`:

```bash
python scripts/build.py --wasi --examples
# Generates: greet.wasm and greet_optimized.wasm
```

## Optimization Level

- Default: `-Oz` (maximum size reduction)
- Also applies: `--strip-debug` (removes debug info)

## File Size Comparison

Optimization shows before/after size and reduction percentage.

## Manual Optimization

```bash
wasm-opt -Oz --strip-debug input.wasm -o output.wasm
```

## Prerequisites

```bash
# Ubuntu/Debian
sudo apt install binaryen

# macOS
brew install binaryen
```

