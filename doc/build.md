# Build Guide

## Quick Start

```bash
cd cdk-c
python scripts/build.py --wasi --examples
```

## Build Options

- `--wasi`: Build for WASI platform (IC canisters)
- `--examples`: Build example programs
- `--clean`: Clean build artifacts
- `--info`: Show build configuration

## Output

- Library: `build/libic_c_sdk.a`
- WASM files: `build/*.wasm`
- Optimized WASM: `build/*_optimized.wasm`

## Environment Variables

- `WASI_SDK_ROOT`: Path to WASI SDK installation
- `WASI_SDK_VERSION`: WASI SDK version (default: 25.0)
- `IC_WASI_POLYFILL_PATH`: Override polyfill library path
- `WASI2IC_PATH`: Override wasi2ic tool path

