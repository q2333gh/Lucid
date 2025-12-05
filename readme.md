> **⚠️ Under Heavy Development**  
> This project is currently under active development. Many features are still in development and APIs may change without notice. Documentation updates may lag behind the latest features—please expect some information to be incomplete or outdated for now.
---

# Lucid : Internet Computer Canister development kit for C, focused on clarity, simplicity, and ease of understanding.


A pure C language SDK for developing Internet Computer (IC) canisters. This SDK provides a lightweight, with minimal dependencies and fast compilation. 

## Features

- **Pure C Implementation**: minimal runtime overhead
- **WASI Support**: Compile to WebAssembly for IC canisters
- **High-level API**: Easy-to-use API for common IC operations
- **Candid Support**: Built-in Candid serialization/deserialization
- **Automatic Optimization**: Built-in WASM optimization with wasm-opt

## Getting Started

### Prerequisites

- **Python 3**: Required for build scripts

### Building the SDK

```bash

# Build with hello examples
python build.py --icwasm 

```

### Quick Example

Create a simple canister function:

```c
#include "ic_c_sdk.h"

IC_EXPORT_QUERY(greet) {
    ic_api_t *api = ic_api_init(IC_ENTRY_QUERY, __func__, false);
    if (api == NULL) {
        ic_api_trap("Failed to initialize IC API");
    }
    
    // Send response
    ic_api_to_wire_text(api, "Hello from C!");
    
    ic_api_free(api);
}
```

Build and test:

```bash
# Build WASM
python build.py --icwasm

# Run tests with PocketIC
python examples/hello_lucid/test/t1_candid.py
```

## Project Structure

```
Lucid/
├── build.py         # Main build script (root level)
├── cdk-c/
│   ├── include/     # Header files
│   │   ├── ic_c_sdk.h   # Main SDK header
│   │   ├── ic_api.h     # High-level API
│   │   ├── ic_candid.h  # Candid support
│   │   └── ...
│   ├── src/        # SDK source files
│   └── scripts/    # Build utilities
│       └── build_utils/ # Build utility modules
├── examples/       # Example canisters
│   └── hello_lucid/ # Hello world example
├── build/          # Native build output directory
└── build-wasi/     # WASI build output directory
```

## Build Options

The build script supports the following options:

```bash
# Build native library (for testing, runs CTest automatically)
python build.py

# Build WASI library (for IC deployment)
python build.py --icwasm
```

The `--icwasm` flag builds IC-compatible WASM canisters with automatic post-processing (wasi2ic conversion, wasm-opt optimization, and Candid interface extraction).

## API Overview

### Entry Points

Use macros to export canister functions:

```c
IC_EXPORT_QUERY(function_name) { ... }   // Query function
IC_EXPORT_UPDATE(function_name) { ... }   // Update function (under development)
```

### High-level API

```c
// Initialize API
ic_api_t *api = ic_api_init(IC_ENTRY_QUERY, __func__, false);

// Get caller principal
ic_principal_t caller = ic_api_get_caller(api);

// Send/receive Candid data
ic_api_to_wire_text(api, "response");
ic_api_from_wire_text(api, &text, &len);

// Cleanup
ic_api_free(api);
```

## Testing

This project uses PocketIC for local testing. PocketIC provides a fast, deterministic IC replica for testing canisters.

### Running Tests

```bash
# Build the canister first
python build.py --icwasm

# Run tests
python examples/hello_lucid/test/t1_candid.py
```

The test script will:
- Automatically set up PocketIC
- Create and install the canister
- Test canister functions using Candid interface
- Verify responses and behavior

## Examples

See `examples/hello_lucid/` for a complete example canister with PocketIC tests.

## Documentation

- API documentation: See header files in `cdk-c/include/`
- Build system: Run `python build.py --help` or see `doc/build.md`
- Examples: See `examples/` directory

## License
Apache 2.0

