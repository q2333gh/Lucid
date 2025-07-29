#!/bin/bash

# Simple run script for pure WASM add function
export PATH="$HOME/.wasmtime/bin:$PATH"

echo "=== Pure WASM Add Function ==="
echo "Building and testing the pure add function..."
echo ""

# Build if needed
if [ ! -f "add.wasm" ] || [ "add.c" -nt "add.wasm" ]; then
    echo "Building add.wasm..."
    make clean && make
    echo ""
fi

# Run the test
echo "Running tests..."
./test.sh