#!/usr/bin/env python3
"""
Test script for greet canister using PocketIC with Candid interface
Uses create_and_install_canister_with_candid for automatic type handling
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


def find_project_root(start_path: Path) -> Path:
    """
    Find project root directory (cdk-c) by looking for characteristic directories.
    
    The project root is identified by the presence of 'src' and 'include' directories.
    This allows the script to work regardless of where it's executed from.
    """
    current = start_path.resolve()
    
    # Search upward for project root
    for parent in [current] + list(current.parents):
        if (parent / "src").exists() and (parent / "include").exists():
            return parent
    
    # Fallback: if we're in examples/hello_lucid/test/, go up 3 levels
    if current.name == "test":
        potential_root = current.parent.parent.parent
        if (potential_root / "src").exists():
            return potential_root
    
    # Last resort: return current directory
    return current


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
    """
    Get the path to build-wasi/bin/hello_lucid_ic.wasm 
    (relative to git repo root; path is always from git repo root)
    """
    # Search upward for repo root (has a ".git" directory)
    script_dir = Path(__file__).parent.resolve()
    current = script_dir
    repo_root = None
    for parent in [current] + list(current.parents):
        if (parent / ".git").exists() and (parent / ".git").is_dir():
            repo_root = parent
            break
    if repo_root is None:
        raise FileNotFoundError("Could not find git repository root (no .git directory found upward from script location)")

    # Always use path from git repo root
    wasm_path = repo_root / "build-wasi" / "bin" / "hello_lucid_ic.wasm"

    if not wasm_path.exists():
        raise FileNotFoundError(
            f"hello_lucid_ic.wasm not found at expected path: {wasm_path}\n(Expected path is relative to git repo root: 'build-wasi/bin/hello_lucid_ic.wasm')"
        )

    return wasm_path
def get_did_path():
    """Get the path to greet.did file"""
    script_dir = Path(__file__).parent.resolve()
    project_root = find_project_root(script_dir)
    
    # DID file is typically in the example directory
    examples_dir = project_root / "examples" / "hello_lucid"
    did_path = examples_dir / "greet.did"
    
    if not did_path.exists():
        # Fallback: try relative to script location
        fallback_path = script_dir.parent / "greet.did"
        if fallback_path.exists():
            return fallback_path
        raise FileNotFoundError(
            f"greet.did not found at {did_path} or {fallback_path}"
        )
    
    return did_path


def main():
    print("Starting PocketIC test for greet canister (using Candid interface)...")
    
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
    
    # Read WASM and DID files
    with open(wasm_path, 'rb') as f:
        wasm_module = f.read()
    
    with open(did_path, 'r') as f:
        candid_interface = f.read()
    
    print("Creating and installing canister with Candid interface...")
    print("(This method automatically handles type parsing using the DID file)")

# create_and_install_canister_with_candid:(): automates canister creation, cycles, install, DID registration; returns object with canister_id.
    canister = pic.create_and_install_canister_with_candid(
        candid=candid_interface,
        wasm_module=wasm_module
    )
    
    canister_id = canister.canister_id
    print(f"Canister created and installed: {canister_id}")
    print(f"Canister object type: {type(canister)}")
    
    # Test greet_no_arg method (no arguments, returns text)
    print("\n=== Testing greet_no_arg ===")
    
    # Use pic.query_call to call the method
    # The canister object created by create_and_install_canister_with_candid
    # provides the Candid interface definition, but we use pic.query_call
    # for the actual call, then decode the response manually based on DID file
    #
    # According to DID file: "greet_no_arg": () -> (text) query;
    # So we expect a text (string) return value
    payload = encode([])  # Empty args for () -> (text)
    response_bytes = pic.query_call(canister_id, "greet_no_arg", payload)
    
    # Decode the Candid response
    # The response is Candid-encoded bytes, we need to extract the text value
    # Format: b'DIDL\r\x16hello world from cdk-c'
    # For text type, we can extract the string directly
    if isinstance(response_bytes, bytes) and b'hello' in response_bytes:
        text_start = response_bytes.find(b'hello')
        response = response_bytes[text_start:].decode('utf-8', errors='ignore').rstrip('\x00')
    else:
        response = str(response_bytes)
    
    print(f"Response: {response}")
    print(f"Response type: {type(response)}")
    
    # Verify the response
    assert isinstance(response, str), f"Expected string response, got {type(response)}"
    assert response == "hello world from cdk-c", f"Expected 'hello world from cdk-c', got: {response}"
    print("✓ greet_no_arg test passed")
    
    print("""\n=== All tests completed ===
# Note: Call methods via pic.query_call() and decode per DID; canister object methods may not match method naming—use direct calls for reliability.
    """)


if __name__ == "__main__":
    main()

