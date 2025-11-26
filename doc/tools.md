# Build Tools

## Required Tools

- **WASI SDK**: Compiler toolchain for WebAssembly
- **Python 3**: Required for build scripts  
  - It is recommended to use **uv** as your Python virtual environment and package manager (see [uv documentation](https://github.com/astral-sh/uv))
- **Python package: sh** (install via `pip install sh` or with `uv pip install sh`)
- **wasm-opt** (optional): For WASM optimization

## Build Scripts

- `scripts/build.py`: Main build script
- `scripts/build_utils/`: Build utilities module
  - `config.py`: Configuration management
  - `utils.py`: Utility functions
  - `verification.py`: WASM verification

## Dependency Tools

- **wasi2ic**: Converts WASI WASM to IC-compatible WASM
- **libic_wasi_polyfill**: WASI→IC system call conversion layer

Both are automatically built if not found.

