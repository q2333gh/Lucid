# WASM Add Function - Meson Build System

This project demonstrates how to compile a simple C add function to WebAssembly using **Meson build system** with WASI SDK.

## 🚀 Quick Start

```bash
# Build minimal version and run tests
python3 build.py --minimal --test

# Build full WASI version  
python3 build.py --full

# Generate WAT files for inspection
python3 build.py --minimal --wat
python3 build.py --full --wat

# View all options
python3 build.py --help
```

## 📁 Project Structure

```
├── add.c                    # Simple C add function
├── meson.build             # Main build configuration
├── meson_options.txt       # Build options
├── build.py               # Modern Python build script
├── cross/                 # Cross-compilation configs (unused)
│   ├── wasi.ini          # WASI target configuration
│   └── wasm32.ini        # Pure WASM32 configuration
├── build_full/           # Full WASI build output
└── build_minimal/        # Minimal build output
```

## 🎯 Two Build Modes

### 1. Minimal Mode (`--minimal`)
- **Size**: ~186 bytes
- **WAT Lines**: ~10 lines
- **Features**: Pure function, no WASI runtime
- **Use Case**: Embedding in other applications

### 2. Full WASI Mode (`--full`)
- **Size**: ~22KB  
- **WAT Lines**: ~7,000 lines
- **Features**: Complete WASI runtime, all system calls
- **Use Case**: Standalone applications

## 🛠️ Prerequisites

1. **Python 3.7+** (for the build script)

2. **Meson Build System**
   ```bash
   pip install meson ninja
   ```

3. **WASI SDK 21.0** (auto-detected in `wasi-sdk-21.0/`)
   ```bash
   wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-21/wasi-sdk-21.0-linux.tar.gz
   tar xvf wasi-sdk-21.0-linux.tar.gz
   ```

4. **wasmtime** (for testing)
   ```bash
   curl https://wasmtime.dev/install.sh -sSf | bash
   ```

## 🧪 Testing

Automated tests are built into the Meson system:

```bash
# Run all tests for minimal build
meson test -C build_minimal

# Run specific test
meson test -C build_minimal add_5_3

# Verbose test output
meson test -C build_minimal --verbose

# Or use the Python script
python3 build.py --minimal --test
```

**Test Cases:**
- `add(5, 3) = 8` ✅
- `add(10, 20) = 30` ✅  
- `add(100, 200) = 300` ✅
- `add(-5, 8) = 3` ✅

## 📊 Size Comparison

| Build Mode | File Size | WAT Lines | Description |
|------------|-----------|-----------|-------------|
| **Minimal** | 186 bytes | 10 lines | Pure function only |
| **Full WASI** | 22KB | 7,022 lines | Complete runtime |

**Minimal WAT Output:**
```wat
(module
  (type (;0;) (func (param i32 i32) (result i32)))
  (func $add (type 0) (param i32 i32) (result i32)
    local.get 1
    local.get 0
    i32.add)
  (memory (;0;) 2)
  (global $__stack_pointer (mut i32) (i32.const 66560))
  (export "memory" (memory 0))
  (export "add" (func $add)))
```

## ⚙️ Manual Meson Usage

### Build Minimal Version
```bash
meson setup build_minimal -Dminimal=true
meson compile -C build_minimal
meson test -C build_minimal
```

### Build Full WASI Version  
```bash
meson setup build_full -Dminimal=false
meson compile -C build_full
meson test -C build_full
```

### Generate WAT Files
```bash
meson compile -C build_minimal wat
meson compile -C build_full wat
```

## 🔧 Configuration Options

- `minimal` (boolean, default: false)
  - Build minimal WASM without WASI runtime
- `wasi_sdk_path` (string, default: auto-detect)
  - Custom WASI SDK installation path

## 🧹 Cleanup

```bash
# Clean all build directories
python3 build.py --clean

# Or manually
rm -rf build_minimal build_full
```

## 🐍 Why Python Build Script?

The Python build script (`build.py`) offers several advantages over the previous bash script:

### ✅ **Improvements**
- **Cross-platform**: Works on Windows, macOS, Linux
- **Better error handling**: Detailed error messages and recovery suggestions
- **Type safety**: Static typing with proper data structures
- **Maintainable**: Clean OOP design with separated concerns
- **Extensible**: Easy to add new features and build modes
- **Dependency checking**: Automatic validation of required tools

### 📊 **Comparison**

| Feature | bash script | Python script |
|---------|-------------|---------------|
| **Lines of code** | 146 lines | ~280 lines (but more readable) |
| **Error handling** | Basic | Comprehensive |
| **Cross-platform** | ❌ Unix only | ✅ All platforms |
| **Maintainability** | ⚠️ String-heavy | ✅ Structured classes |
| **Extensibility** | ❌ Difficult | ✅ Easy to extend |
| **Type safety** | ❌ None | ✅ Full typing |
| **Dependency check** | ❌ Manual | ✅ Automatic |

## 📚 Additional Documentation

- [`README_MESON.md`](README_MESON.md) - Detailed Meson usage guide
- [`MESON_RESULTS.md`](MESON_RESULTS.md) - Migration results and comparison

## 🎉 Why This Stack?

- ✅ **Python**: Modern, readable, cross-platform
- ✅ **Meson**: Fast, parallel builds with great cross-compilation
- ✅ **WASI SDK**: Industry-standard WebAssembly toolchain
- ✅ **Structured**: Clean separation of concerns
- ✅ **Testable**: Built-in automated testing

---

**A modern, maintainable approach to WebAssembly development.**