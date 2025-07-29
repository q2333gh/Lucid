# WebAssembly Add Function

This project demonstrates how to compile a simple C add function to WebAssembly (WASM) using Emscripten.

## Files

- `add.c` - C source code with the add function
- `Makefile` - Build configuration for compiling to WASM
- `index.html` - Test page to demonstrate the WASM function
- `run.sh` - Convenient script to compile and run the project
- `README.md` - This file

## Quick Start

The easiest way to run this project:

```bash
./run.sh
```

This will:
1. Set up the Emscripten environment
2. Compile the C code to WebAssembly (if needed)
3. Start a local HTTP server
4. Open your browser to test the function

## Prerequisites

You need to install Emscripten to compile C code to WebAssembly:

### Option 1: Install Emscripten using the Makefile
```bash
make install-emscripten
```

### Option 2: Manual installation
```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

## Building

1. Make sure Emscripten is installed and activated
2. Run the build command:
```bash
make
```

This will generate:
- `add.js` - JavaScript glue code
- `add.wasm` - WebAssembly binary

## Testing

### Option 1: Using the run script (Recommended)
```bash
./run.sh
```

### Option 2: Manual testing
1. After building, open `index.html` in a web browser
2. Enter two numbers in the input fields
3. Click "Add Numbers" to see the result from the WASM function

## How it works

The C function `add_exported` is compiled to WebAssembly and can be called from JavaScript using the `ccall` method:

```javascript
const result = wasmModule.ccall('add_exported', 'number', ['number', 'number'], [num1, num2]);
```

## Clean up

To remove generated files:
```bash
make clean
```

## Notes

- The function uses `EMSCRIPTEN_KEEPALIVE` to prevent the function from being optimized away
- The Makefile exports the function and runtime methods needed for JavaScript interaction
- The HTML page includes error handling and a user-friendly interface
- The `run.sh` script automatically handles environment setup and compilation