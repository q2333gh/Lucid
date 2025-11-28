# IC C SDK Build Process

This document summarizes how `cdk-c/scripts/build.py` orchestrates builds for the IC C SDK library and its examples.

## High-Level Flow

1. **Entry point (`run`)**
   - Typer parses CLI flags (`--build`, `--clean`, `--examples`, `--info`, `--wasi`).
   - Paths are initialized per invocation; `ICBuilder` receives the configuration and desired platform.
2. **Library build**
   - `ICBuilder.ensure_directories` creates the appropriate build directory (`build/` for native, `build-wasi/` for WASI).
   - `check_source_files_exist` validates that every file in `LIB_SOURCES` is present under `src/`.
   - Each source is compiled through `ICBuilder.compile_source`, which wraps the platform-specific compiler plus shared flags from `get_compile_flags`.
   - `ICBuilder.create_static_lib` archives all objects via the configured `ar`, producing `libic_native.a` or `libic_wasi.a` (exact name determined by `get_library_name`).
3. **Example build**
   - Examples are enumerated from `examples/*.c`.
   - Each example is compiled with the same helper; the resulting object lives in the platform-specific build directory.
   - **Native**: Build stops at the object file, leaving linkage to downstream consumers.
   - **WASI**: `_process_wasi_artifact` links the example against the library and the polyfill, verifies the required `raw_init` import, converts the result using `wasi2ic`, and optionally runs `wasm-opt`.

## Supporting Utilities

- `initialize_paths` centralizes filesystem layout.
- `get_wasi_sdk_paths` discovers toolchain binaries when `--wasi` is set.
- `ensure_polyfill_library` and `ensure_wasi2ic_tool` lazily build third-party helpers at pinned commits.
- `needs_rebuild` prevents stale artifacts when building examples.

## Typical Commands

- `./build.py --build` — compile the core library for native targets.
- `./build.py --build --wasi` — produce the WASI static library and supporting IC-compatible artifacts.
- `./build.py --examples --wasi` — rebuild the library if required, then emit WASM bundles for every example.
- `./build.py --clean` — remove both native and WASI build directories.
- `./build.py --info --wasi` — display the active configuration along with polyfill and `wasi2ic` status.

Use `--help` to see the full set of switches exposed by Typer.


(cdk-c lib + example +  c_candid)->