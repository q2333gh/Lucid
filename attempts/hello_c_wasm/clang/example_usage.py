#!/usr/bin/env python3
"""
Example: Using pure WASM add function from Python

This demonstrates how to use the pure add.wasm module
from Python using wasmtime-py library.

Install wasmtime-py:
    pip install wasmtime-py

Or use subprocess to call wasmtime directly (shown below).
"""

import subprocess
import sys

def call_wasm_add(a, b):
    """Call the pure WASM add function using subprocess"""
    try:
        result = subprocess.run([
            'wasmtime', '--invoke', 'add', 'add.wasm', str(a), str(b)
        ], capture_output=True, text=True, cwd='.')
        
        if result.returncode == 0:
            return int(result.stdout.strip())
        else:
            print(f"Error: {result.stderr}")
            return None
    except FileNotFoundError:
        print("Error: wasmtime not found. Please install wasmtime.")
        return None

def main():
    print("=== Python + Pure WASM Add Function ===")
    print("This shows how to use pure WASM functions from other languages")
    print()
    
    # Test cases
    test_cases = [
        (5, 3),
        (10, 20),
        (100, 200),
        (-5, 8),
        (0, 0),
        (999, 1)
    ]
    
    print("Testing WASM add function from Python:")
    print("=====================================")
    
    for a, b in test_cases:
        result = call_wasm_add(a, b)
        if result is not None:
            print(f"add({a}, {b}) = {result}")
        else:
            print(f"Failed to call add({a}, {b})")
    
    print()
    print("=== Benefits of Pure WASM Functions ===")
    print("1. Language agnostic - can be called from any language")
    print("2. Sandboxed execution - safe to run untrusted code")
    print("3. High performance - near-native speed")
    print("4. Portable - runs on any platform with WASM runtime")
    print("5. Composable - can be combined with other WASM modules")

if __name__ == "__main__":
    main()