> **⚠️ Under Heavy Development**  
> This project is currently under active development. Many features are still in development and APIs may change without notice. Documentation updates may lag behind the latest features—please expect some information to be incomplete or outdated for now.
---

# Lucid : Internet Computer Canister development kit for C, focused on clarity, simplicity, and ease of understanding.


A pure C language SDK for developing Internet Computer (IC) canisters. This SDK provides a lightweight, with minimal dependencies and fast compilation. 

## Features

- **Pure C Implementation**: minimal runtime overhead and precise control
- **WASI Support**: Compile to IC compatible wasm module
- **High-level API**: Easy-to-use API for common IC operations
- **Candid Support**: Built-in Candid serialization/deserialization

## Getting Started

### Prerequisites

- **Python 3**: Required for build scripts

### Building the SDK

```bash

# Build with hello examples
python build.py --new hello_lucid
python build.py --icwasm --examples hello_lucid

```

### Quick Example

Create a simple canister function:

```c
#include "ic_c_sdk.h"

IC_CANDID_EXPORT_DID()

IC_API_QUERY(greet, "() -> (text)") {
    // api variable is automatically initialized
    IC_API_REPLY_TEXT("Hello from C!");
    // api is automatically freed when function returns
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
│   │   ├── ic_simple.h  # Simplified API macros
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

### Simplified API (Recommended)

The `IC_API_QUERY` and `IC_API_UPDATE` macros automatically handle API initialization and cleanup:

```c
IC_API_QUERY(my_query, "() -> (text)") {
    // api is automatically initialized and available
    ic_principal_t caller = ic_api_get_caller(api);
    
    // Convenient reply macros
    IC_API_REPLY_TEXT("response");
    IC_API_REPLY_NAT(42);
    IC_API_REPLY_INT(-10);
    IC_API_REPLY_EMPTY();
    
    // api is automatically freed when function returns
}
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

See `examples/hello_lucid/` for a complete example canister with ic-py or local testnet replica tests.

## Documentation

- API documentation: See header files in `cdk-c/include/`
- Build system: Run `python build.py --help` or see `doc/build.md`
- Examples: See `examples/` directory

## License
Apache 2.0

