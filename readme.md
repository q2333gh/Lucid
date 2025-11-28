> **⚠️ Under Heavy Development**  
> This project is currently under active development. Many features are still in development and APIs may change without notice. 
---

# Lucid : Internet Computer Canister development kit for C, focused on clarity, simplicity, and ease of understanding.


A pure C language SDK for developing Internet Computer (IC) canisters. This SDK provides a lightweight, with minimal dependencies and fast compilation. 

## Features

- **Pure C Implementation**: minimal runtime overhead
- **WASI Support**: Compile to WebAssembly for IC canisters
- **High-level API**: Easy-to-use API for common IC operations
- **Candid Support**: Built-in Candid serialization/deserialization
- **Principal Handling**: Type-safe principal operations
- **Automatic Optimization**: Built-in WASM optimization with wasm-opt

## Getting Started

### Prerequisites

- **Python 3**: Required for build scripts

### Building the SDK

```bash

# Build with hello examples
python build.py --wasi 

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
python build.py --wasi --examples

# Run tests with PocketIC
python examples/hello_lucid/test/t1_candid.py
```

## Project Structure

```
cdk-c/
├── include/          # Header files
│   ├── ic_c_sdk.h   # Main SDK header
│   ├── ic_api.h     # High-level API
│   ├── ic_candid.h  # Candid support
│   └── ...
├── src/             # SDK source files
├── examples/        # Example canisters
│   └── hello_lucid/ # Hello world example
├── scripts/         # Build scripts
│   ├── build.py     # Main build script
│   └── build_utils/ # Build utilities
└── build/           # Build output directory
```

## Build Options

The build script supports multiple options:

```bash
# Build native library (for testing)
python scripts/build.py

# Build WASI library (for IC deployment)
python scripts/build.py --wasi

# Build examples
python scripts/build.py --examples

# Clean build artifacts
python scripts/build.py --clean

# Show build configuration
python scripts/build.py --info
```

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
python scripts/build.py --wasi --examples

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

- API documentation: See header files in `include/`
- Build system: See `scripts/build.py --help`
- Examples: See `examples/` directory

## License
Apache 2.0

