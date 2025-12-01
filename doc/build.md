## Build Guide

### Quick Start

From the repository root:

```bash
python build.py          # Native host build + run CTest
python build.py --wasi   # WASI / IC canister build
```

### Build Options

- **No flag (default)** – Native (host) build
  - Produces a native static library
  - Enables and runs the CTest test suite
- **`--wasi`** – WASI / IC canister build
  - Uses the WASI SDK and CMake toolchain
  - Automatically builds `libic_wasi_polyfill.a` and the `wasi2ic` tool if they are missing
  - Post-processes generated `.wasm` files to `_ic.wasm` and verifies imports

### Output Directories and Artifacts

- **Native build**
  - Build directory: `build/`
  - Static library and other artifacts: under `build/`
  - `compile_commands.json` is copied to the project root for IDE support

- **WASI build (`--wasi`)**
  - Build directory: `build-wasi/`
  - Helper libraries and tools directory: `build_lib/`
    - `build_lib/libic_wasi_polyfill.a`
    - `build_lib/wasi2ic`
  - WASM artifacts:
    - Original WASM: `build-wasi/bin/*.wasm`
    - Converted IC WASM: `build-wasi/bin/*_ic.wasm`

### Environment Variables

- **`WASI_SDK_ROOT`** – Path to your WASI SDK installation  
  - `build.py` locates `share/cmake/wasi-sdk.cmake` (or the same file under a versioned subdirectory) based on this path
  - If the SDK or toolchain file cannot be found, the `--wasi` build will fail and exit

More advanced WASI SDK version and path control (for example via `WASI_SDK_VERSION`) is handled internally by the `cdk-c/scripts/build_utils` module. In most cases, setting `WASI_SDK_ROOT` correctly is sufficient to build successfully.
