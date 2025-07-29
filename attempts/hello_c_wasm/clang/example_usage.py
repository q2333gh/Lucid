#!/usr/bin/env python3
"""
Example: Using WASM add function from Python

This demonstrates how to use the WASM add module built with Meson
from Python using wasmtime subprocess calls.

Prerequisites:
    - Build the WASM module first: ./build.sh --minimal
    - Or: meson setup build_minimal -Dminimal=true && meson compile -C build_minimal
"""

import subprocess
import sys
import os

def call_wasm_add(a, b, use_minimal=True):
    """Call the WASM add function using subprocess"""
    # Choose which WASM file to use
    if use_minimal:
        wasm_file = "build_minimal/add_minimal.wasm"
        file_desc = "minimal"
    else:
        wasm_file = "build_full/add.wasm"
        file_desc = "full WASI"
    
    # Check if WASM file exists
    if not os.path.exists(wasm_file):
        print(f"❌ WASM file not found: {wasm_file}")
        print(f"Please build it first:")
        if use_minimal:
            print("   ./build.sh --minimal")
        else:
            print("   ./build.sh --full")
        return None
    
    try:
        result = subprocess.run([
            'wasmtime', '--invoke', 'add', wasm_file, str(a), str(b)
        ], capture_output=True, text=True, timeout=5)
        
        if result.returncode == 0:
            return int(result.stdout.strip())
        else:
            print(f"❌ Error calling WASM function: {result.stderr}")
            return None
    except subprocess.TimeoutExpired:
        print("❌ Timeout calling WASM function")
        return None
    except FileNotFoundError:
        print("❌ wasmtime not found. Please install it:")
        print("   curl https://wasmtime.dev/install.sh -sSf | bash")
        return None
    except Exception as e:
        print(f"❌ Error: {e}")
        return None

def main():
    print("🚀 WASM Add Function - Python Integration Example")
    print("=" * 50)
    
    # Test cases
    test_cases = [
        (5, 3),
        (10, 20),
        (100, 200),
        (-5, 8),
        (0, 0),
        (999, 1)
    ]
    
    # Test both minimal and full versions
    for version, use_minimal in [("Minimal", True), ("Full WASI", False)]:
        print(f"\n📊 Testing {version} Version:")
        print("-" * 30)
        
        success_count = 0
        for a, b in test_cases:
            result = call_wasm_add(a, b, use_minimal)
            if result is not None:
                expected = a + b
                status = "✅" if result == expected else "❌"
                print(f"{status} add({a}, {b}) = {result} (expected: {expected})")
                if result == expected:
                    success_count += 1
            else:
                print(f"❌ add({a}, {b}) = FAILED")
        
        print(f"\nResults: {success_count}/{len(test_cases)} tests passed")
        
        if success_count == 0:
            break  # Don't test the other version if this one failed

    print("\n🎯 Performance & Size Comparison:")
    print("-" * 35)
    
    # Show file sizes if both exist
    minimal_path = "build_minimal/add_minimal.wasm"
    full_path = "build_full/add.wasm"
    
    if os.path.exists(minimal_path):
        minimal_size = os.path.getsize(minimal_path)
        print(f"📦 Minimal version: {minimal_size} bytes")
    
    if os.path.exists(full_path):
        full_size = os.path.getsize(full_path)
        print(f"📦 Full WASI version: {full_size:,} bytes")
        
        if os.path.exists(minimal_path):
            ratio = full_size / minimal_size
            print(f"📈 Size ratio: {ratio:.1f}x larger")

    print("\n💡 Integration Tips:")
    print("- Use minimal version for embedding in applications")
    print("- Use full WASI version for standalone tools")
    print("- Both versions provide identical functionality")
    print("- WASM provides sandboxed, cross-platform execution")

if __name__ == "__main__":
    main()