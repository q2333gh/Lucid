# Build Guide

## Prerequisites

### System Dependencies
```bash
sudo apt-get install -y cmake clang lld ninja-build binaryen libcriterion-dev
```

### Python Dependencies
```bash
pip install -r requirements.txt
```

### WASI SDK (Required for IC builds)
Download from: https://github.com/WebAssembly/wasi-sdk/releases

```bash
export WASI_SDK_ROOT=/path/to/wasi-sdk-25.0-x86_64-linux
```

## Build Commands

```bash
# Native build + tests
python build.py

# IC canister build (WASM)
python build.py --icwasm

# Build specific example
python build.py --icwasm --examples types_example
```

## Build Options

- **Default** - Native library + CTest
- **`--icwasm`** - WASI/IC canister (requires WASI SDK)
- **`--examples NAME`** - Build specific example

## Output Directories

- **Native**: `build/`
- **WASI**: `build-wasi/`
  - WASM: `build-wasi/bin/*.wasm`
  - IC WASM: `build-wasi/bin/*_ic.wasm`
- **Libraries**: `build_lib/`

## Troubleshooting

**WASI SDK not found:**
```bash
# Set correct path
export WASI_SDK_ROOT=/path/to/wasi-sdk
```

**Build fails:**
```bash
# Clean and rebuild
rm -rf build-wasi
python build.py --icwasm
```
