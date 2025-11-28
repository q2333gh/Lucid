#!/usr/bin/env python3
"""
Test script for greet canister using PocketIC
Tests the greet.wasm file in the parent directory
"""

import sys
import os
from pathlib import Path

try:
    from pocket_ic import PocketIC
    from ic.candid import encode
except ImportError:
    print("Error: pocket-ic not installed. Install it with: pip3 install pocket-ic")
    print("Or: pip install pocket-ic")
    sys.exit(1)


def setup_pocketic_binary():
    """Set up PocketIC binary path if not already set"""
    # Get the directory where this script is located
    script_dir = Path(__file__).parent.resolve()
    pocket_ic_bin = script_dir / "pocket-ic"
    
    # If POCKET_IC_BIN is not set and pocket-ic exists in script directory, set it
    if "POCKET_IC_BIN" not in os.environ and pocket_ic_bin.exists():
        os.environ["POCKET_IC_BIN"] = str(pocket_ic_bin)
        print(f"Set POCKET_IC_BIN to: {pocket_ic_bin}")
    elif "POCKET_IC_BIN" not in os.environ:
        # Check if pocket-ic exists in current working directory
        cwd_pocket_ic = Path.cwd() / "pocket-ic"
        if cwd_pocket_ic.exists():
            os.environ["POCKET_IC_BIN"] = str(cwd_pocket_ic)
            print(f"Set POCKET_IC_BIN to: {cwd_pocket_ic}")
        else:
            print(f"Warning: pocket-ic binary not found in {script_dir} or {Path.cwd()}")
            print("Please set POCKET_IC_BIN environment variable or place pocket-ic in the test directory")


def get_wasm_path():
    """Get the path to greet.wasm file"""
    script_dir = Path(__file__).parent
    # Try optimized version first, fallback to original
    wasm_path = script_dir.parent / "greet_optimized.wasm"
    if not wasm_path.exists():
        wasm_path = script_dir.parent / "greet.wasm"
    
    if not wasm_path.exists():
        raise FileNotFoundError(f"greet.wasm not found at {script_dir.parent / 'greet.wasm'}")
    
    return wasm_path


def get_did_path():
    """Get the path to greet.did file"""
    script_dir = Path(__file__).parent
    did_path = script_dir.parent / "greet.did"
    
    if not did_path.exists():
        raise FileNotFoundError(f"greet.did not found at {did_path}")
    
    return did_path


def main():
    print("Starting PocketIC test for greet canister...")
    
    # Set up PocketIC binary path before initializing PocketIC
    setup_pocketic_binary()
    
    # Get paths
    wasm_path = get_wasm_path()
    did_path = get_did_path()
    
    print(f"WASM file: {wasm_path}")
    print(f"DID file: {did_path}")
    
    # Initialize PocketIC
    pic = PocketIC()
    print("PocketIC initialized")
    
    # Create a canister
    canister_id = pic.create_canister()
    print(f"Created canister: {canister_id}")
    
    # Add cycles to the canister
    pic.add_cycles(canister_id, 2_000_000_000_000)  # 2T cycles
    print("Added cycles to canister")
    
    # Read WASM file
    with open(wasm_path, 'rb') as f:
        wasm_module = f.read()
    
    print("Installing canister code...")
    # Install the canister code (third parameter is init args, empty list for no args)
    pic.install_code(canister_id, bytes(wasm_module), [])
    print("Canister code installed successfully")
    
    # Test greet_no_arg method (no arguments, returns text)
    print("\n=== Testing greet_no_arg ===")
    # For query calls with no arguments, payload needs to be candid-encoded bytes
    # Empty list means no arguments
    payload = encode([])
    response_bytes = pic.query_call(canister_id, "greet_no_arg", payload)
    print(f"Raw response: {response_bytes}")
    
    # Extract string from Candid response
    # Format: b'DIDL\r\x16hello world from cdk-c'
    # Structure: 'DIDL' (4) + return count (1) + type table + values
    # For text type: type code 0x16 (22) + LEB128 length + string bytes
    if isinstance(response_bytes, bytes):
        if response_bytes.startswith(b'DIDL'):
            # Find the text string in the response
            # Look for the string directly after the type code
            text_start = response_bytes.find(b'hello')
            if text_start > 0:
                # Extract from 'hello' to the end (or find null terminator)
                text_end = response_bytes.find(b'\x00', text_start)
                if text_end > 0:
                    response = response_bytes[text_start:text_end].decode('utf-8')
                else:
                    # Extract to end, removing any trailing nulls
                    response = response_bytes[text_start:].decode('utf-8', errors='ignore').rstrip('\x00')
            else:
                # Fallback: try to decode the whole thing and extract text
                try:
                    decoded_str = response_bytes.decode('utf-8', errors='ignore')
                    # Find the expected string in the decoded text
                    if 'hello world from cdk-c' in decoded_str:
                        response = 'hello world from cdk-c'
                    else:
                        response = decoded_str.strip('\x00')
                except:
                    response = str(response_bytes)
        else:
            response = response_bytes.decode('utf-8', errors='ignore')
    else:
        response = str(response_bytes)
    
    print(f"Extracted response: {response}")
    assert isinstance(response, str), f"Expected string response, got {type(response)}"
    assert "hello world from cdk-c" in response, f"Expected 'hello world from cdk-c' in response, got: {response}"
    print("✓ greet_no_arg test passed")
    
    print("\n=== All tests completed ===")


if __name__ == "__main__":
    main()

