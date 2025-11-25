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
cmake -S test -B test/build
cmake --build test/build
```

- This generates the `ic_cdk_tests` executable in `test/build`.
- After source changes, rerun `cmake --build test/build` for incremental builds.

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
