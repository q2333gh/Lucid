## Build Guide

### Prerequisites

Before building, ensure you have the following dependencies installed:

#### System Dependencies

- **CMake** (3.x or later) – Build system
- **C Compiler** – Clang (recommended) or GCC
- **Ninja** (optional) – Faster build system (auto-detected if available)
- **Binaryen** (`wasm-opt` tool) – Required for WASM optimization (WASI builds)
- **LLD** – LLVM linker (recommended for better WASM linking)
- **Criterion** (`libcriterion-dev`) – Required for native tests

On Ubuntu/Debian, install with:
```bash
sudo apt-get install -y cmake clang lld ninja-build binaryen libcriterion-dev
```

#### Python Dependencies

- **Python 3.11+** – Required for build scripts
- Python packages (install via `requirements.txt`):
  ```bash
  pip install -r requirements.txt
  ```
  This includes:
  - `pocket_ic` – IC replica for testing
  - `rich` – Terminal formatting
  - `typer` – CLI framework
  - Other build utilities

#### WASI SDK (for `--icwasm` builds only)

- **WASI SDK 25.0** – Required for IC canister builds
- Set `WASI_SDK_ROOT` environment variable to your SDK installation path:
  ```bash
  export WASI_SDK_ROOT=/path/to/wasi-sdk-25.0-x86_64-linux
  ```
- Download from: https://github.com/WebAssembly/wasi-sdk/releases
- The build script will automatically locate the toolchain file at `$WASI_SDK_ROOT/share/cmake/wasi-sdk.cmake`

### Quick Start

From the repository root:

```bash
python build.py          # Native host build + run CTest
python build.py --icwasm # WASI / IC canister build
```

### Build Options

- **No flag (default)** – Native (host) build
  - Produces a native static library
  - Enables and runs the CTest test suite automatically
- **`--icwasm`** – WASI / IC canister build
  - Uses the WASI SDK and CMake toolchain
  - Automatically builds `libic_wasi_polyfill.a` and the `wasi2ic` tool if they are missing
  - Post-processes generated `.wasm` files to `_ic.wasm` and verifies imports
  - Build pipeline includes 4 phases: Toolchain → IC Tools → Build → Post-process

### Output Directories and Artifacts

- **Native build**
  - Build directory: `build/`
  - Static library and other artifacts: under `build/`
  - `compile_commands.json` is copied to the project root for IDE support

- **WASI build (`--icwasm`)**
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
  - If the SDK or toolchain file cannot be found, the `--icwasm` build will fail and exit

More advanced WASI SDK version and path control (for example via `WASI_SDK_VERSION`) is handled internally by the `cdk-c/scripts/build_utils` module. In most cases, setting `WASI_SDK_ROOT` correctly is sufficient to build successfully.
