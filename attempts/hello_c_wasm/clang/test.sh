#!/bin/bash

# Test script for pure WASM add function
export PATH="$HOME/.wasmtime/bin:$PATH"

echo "=== Pure WASM Add Function Test ==="
echo "This demonstrates calling a pure C function compiled to WASM"
echo "without any main() function or command-line interface."
echo ""

echo "Source code (add.c):"
echo "===================="
cat add.c
echo ""

echo "Testing the pure add function:"
echo "=============================="

# Test different values
test_cases=(
    "5 3"
    "10 20" 
    "100 200"
    "-5 8"
    "0 0"
    "999 1"
)

for case in "${test_cases[@]}"; do
    echo -n "add($case) = "
    wasmtime --invoke add add.wasm $case 2>/dev/null
done

echo ""
echo "=== Key Benefits of Pure WASM ==="
echo "1. No main() function needed"
echo "2. Direct function invocation with --invoke"
echo "3. Clean separation of logic and I/O"
echo "4. Can be imported and used by other WASM modules"
echo "5. Smaller binary size and faster loading"
echo ""

echo "File size:"
echo "=========="
echo "add.wasm:                  $(du -h add.wasm | cut -f1)"