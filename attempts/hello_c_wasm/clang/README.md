# WASI Add Function with Clang

This project demonstrates how to compile a C add function to WebAssembly using Clang with WASI (WebAssembly System Interface) target.

## What is WASI?

WASI (WebAssembly System Interface) is a standard interface that allows WebAssembly modules to interact with the host system, including:
- File system access
- Network access
- Console I/O (stdout, stderr)
- Environment variables
- And more

## Files

- `add.c` - C source code with add function and stdout output
- `add_pure.c` - Pure C function without main() or I/O operations
- `Makefile` - Build configuration for Clang WASI compilation
- `Makefile_pure` - Build configuration for pure function
- `run.sh` - Convenient script to compile and test
- `test_pure.sh` - Test script for pure WASM function
- `README.md` - This file

## Two Approaches

### 1. Traditional WASI Approach (add.c)
This approach includes a main() function and uses WASI for command-line arguments and console output:
- Has a main() function that processes command-line arguments
- Uses printf() for output
- Runs like a traditional command-line program
- File size: ~40KB

### 2. Pure Function Approach (add_pure.c)
This approach creates a pure function that can be directly invoked:
- No main() function needed
- No I/O operations (printf, etc.)
- Direct function invocation using `wasmtime --invoke`
- Smaller binary size (~24KB)
- Better for embedding in other applications
- Clean separation of logic and I/O

## Quick Start

### Traditional WASI Approach
The easiest way to run the traditional version:

```bash
./run.sh
```

### Pure Function Approach
To test the pure function version:

```bash
# Build the pure version
make -f Makefile_pure

# Test it
./test_pure.sh

# Or call directly
wasmtime --invoke add add_pure.wasm 5 3
```

## Prerequisites

The project automatically downloads and sets up:
- WASI SDK 21.0 (Clang with WASI support)
- wasmtime runtime

## Building

Compile the C code to WebAssembly:
```bash
make
```

This will generate:
- `add.wasm` - WebAssembly binary with WASI support

## Testing

### Option 1: Using the run script (Recommended)
```bash
./run.sh
```

### Option 2: Manual testing
```bash
# Set up environment
export PATH="$(pwd)/wasi-sdk-21.0/bin:$HOME/.wasmtime/bin:$PATH"

# Test with different numbers
wasmtime add.wasm 5 3
wasmtime add.wasm 10 20
wasmtime add.wasm 100 200
```

## Example Output

When you run `wasmtime add.wasm 5 3`, you should see:
```
WASI: Adding 5 + 3 = 8
Result: 8
```

## How it works

1. **Clang with WASI target**: Uses `--target=wasm32-wasi` to compile for WASI
2. **stdout support**: WASI provides access to standard output
3. **Command line arguments**: WASI allows passing arguments to the program
4. **Runtime execution**: wasmtime provides the WASI runtime environment

## Key Features

- **stdout Output**: The function reports calculations to stdout
- **Command Line Arguments**: Accepts two numbers as arguments
- **Error Handling**: Shows usage instructions if wrong number of arguments
- **Cross-platform**: Runs on any system with wasmtime

## Differences from Emscripten

| Feature | Emscripten | Clang + WASI |
|---------|------------|--------------|
| Target | Browser/Node.js | Standalone runtime |
| I/O | JavaScript APIs | WASI system calls |
| Runtime | Emscripten runtime | wasmtime/wasmer |
| Use case | Web applications | Server-side, CLI tools |
| stdout | JavaScript console.log | Native stdout |

## Clean up

To remove generated files:
```bash
make clean
```

## Notes

- WASI provides a more standard POSIX-like interface
- Better for server-side and CLI applications
- Can run in any WASI-compatible runtime (wasmtime, wasmer, etc.)
- No JavaScript glue code needed
- Direct access to system I/O

## Using Pure WASM Functions from Other Languages

The pure function approach allows easy integration with other programming languages. We provide an example in Python:

```bash
# Run the Python example
python3 example_usage.py
```

This demonstrates:
- Calling WASM functions from Python using subprocess
- Language-agnostic interface
- Sandboxed execution
- High performance computation

You can adapt this pattern for other languages like JavaScript, Rust, Go, etc.

## Summary

**Use Traditional WASI approach when:**
- You need a standalone command-line tool
- You want console I/O and argument processing
- You're building a complete application

**Use Pure Function approach when:**
- You want to embed functions in other applications
- You need language-agnostic interfaces
- You want smaller binary sizes
- You prefer clean separation of logic and I/O
- You're building reusable computational modules

Both approaches are fully supported by wasmtime and provide excellent performance and portability.