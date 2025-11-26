# Criterion Test Suite Guide

This document explains how to build and run the Criterion unit tests for `cdk-c` locally, verifying the behavior of core modules such as `ic_buffer`, `ic_principal`, and `ic_candid`.

## 1. Prerequisites

Ensure the following are installed:

| Component | Description | Ubuntu Installation |
| --- | --- | --- |
| CMake ≥ 3.20 | Build system generator | `sudo apt install cmake` |
| Clang / GCC | C11 compiler (defaults to system `cc`) | `sudo apt install build-essential clang` |
| pkg-config | Dependency discovery for CMake | `sudo apt install pkg-config` |
| Criterion | Unit testing framework | `sudo apt install libcriterion-dev` |

> If no distribution package is available, you can build Criterion from source and expose `criterion.pc` by setting `PKG_CONFIG_PATH=/path/to/criterion/lib/pkgconfig`.

## 2. Configuration and Build

From the repository root:

```bash
cd cdk-c
cmake -S test -B test/build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build test/build
```

- This generates the `ic_cdk_tests` executable in `test/build`.
- After source changes, rerun `cmake --build test/build` for incremental builds.

### Keeping clangd happy

The helper script at the repo root merges everything for you:

```bash
# 1) Generate/refresh the host database (Criterion build)
cd /home/<you>/code/Lucid/cdk-c
cmake -S test -B test/build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# 2) Generate/refresh the WASI database (lands in cdk-c/build/compile_commands.json)
bear --cdb build/compile_commands.json python scripts/build.py --wasi --examples

# 3) Merge from the cdk-c directory
cd /home/<you>/code/Lucid/cdk-c
python scripts/merge_compile_commands.py
```

- The script looks for `cdk-c/build/compile_commands.json` (WASI) and `cdk-c/test/build/compile_commands.json` (host) and writes the merged result to `cdk-c/compile_commands.json`, which clangd/VS Code can consume directly (VS Code is already configured via `.vscode/c_cpp_properties.json`).
- If you need the merged database available at the repo root for other tooling, add `ln -sf cdk-c/compile_commands.json compile_commands.json`.
- Pass `--inputs ...` if you want to add more submodules, `--output` to write elsewhere, or `--keep-duplicates` if you need the raw entries untouched.
- Because the script resolves absolute paths internally, you can run it from any directory; just point `--root` to your workspace if you are not already inside it.

## 3. Running Tests

Using CTest:

```bash
ctest --test-dir test/build          # Concise output
ctest --test-dir test/build -V       # Verbose mode
```

Or run the executable directly:

```bash
test/build/ic_cdk_tests
```

Exit code 0 indicates all assertions passed.

## 4. Troubleshooting

| Symptom | Cause/Solution |
| --- | --- |
| `No package 'criterion' found` | Criterion not installed; use `sudo apt install libcriterion-dev`, or configure `PKG_CONFIG_PATH` to point to a custom installation. |
| `cmake` reports version too low | Upgrade system CMake, or install via `pip install cmake` and add it to `PATH`. |
| Need to start fresh | Remove `test/build` (`rm -rf test/build`) and rerun the configuration steps. |

Following these steps will reproduce the Criterion test workflow established in our conversation. Happy testing!
