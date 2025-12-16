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
    script_dir = Path(__file__).parent.resolve()
    pocket_ic_bin = script_dir / "pocket-ic"

    if "POCKET_IC_BIN" not in os.environ and pocket_ic_bin.exists():
        os.environ["POCKET_IC_BIN"] = str(pocket_ic_bin)
        print(f"Set POCKET_IC_BIN to: {pocket_ic_bin}")
    elif "POCKET_IC_BIN" not in os.environ:
        cwd_pocket_ic = Path.cwd() / "pocket-ic"
        if cwd_pocket_ic.exists():
            os.environ["POCKET_IC_BIN"] = str(cwd_pocket_ic)
            print(f"Set POCKET_IC_BIN to: {cwd_pocket_ic}")
        else:
            print(
                f"Warning: pocket-ic binary not found in {script_dir} or {Path.cwd()}"
            )
            print(
                "Please set POCKET_IC_BIN environment variable or place pocket-ic in the test directory"
            )


def get_wasm_path():
    """Get the path to build-wasi/bin/hello_lucid_ic.wasm"""
    script_dir = Path(__file__).parent.resolve()
    current = script_dir
    repo_root = None
    for parent in [current] + list(current.parents):
        if (parent / ".git").exists() and (parent / ".git").is_dir():
            repo_root = parent
            break
    if repo_root is None:
        raise FileNotFoundError(
            "Could not find git repository root (no .git directory found upward from script location)"
        )

    wasm_path = repo_root / "build-wasi" / "bin" / "hello_lucid_ic.wasm"

    if not wasm_path.exists():
        raise FileNotFoundError(
            f"hello_lucid_ic.wasm not found at expected path: {wasm_path}\n"
            "(Expected path is relative to git repo root: 'build-wasi/bin/hello_lucid_ic.wasm')"
        )

    return wasm_path


def get_did_path():
    """Get the path to hello_lucid.did file"""
    script_dir = Path(__file__).parent.resolve()
    project_root = find_project_root(script_dir)

    examples_dir = project_root / "examples" / "hello_lucid"
    did_path = examples_dir / "hello_lucid.did"

    if not did_path.exists():
        fallback_path = script_dir.parent / "hello_lucid.did"
        if fallback_path.exists():
            return fallback_path
        raise FileNotFoundError(
            f"hello_lucid.did not found at {did_path} or {fallback_path}"
        )

    return did_path


def decode_candid_text(response_bytes: bytes) -> str:
    """
    Simple manual decoding for Candid text responses.
    Extracts printable string from DIDL encoded bytes.
    """
    if not isinstance(response_bytes, bytes):
        return str(response_bytes)

    # Try to find known string pattern or just extract printable chars
    # This is a naive implementation for test purposes
    try:
        # Look for DIDL magic bytes
        if response_bytes.startswith(b"DIDL"):
            # Skip header roughly (magic + table) and look for content
            # Just extraction of ASCII/UTF-8 content for now
            import re

            text_content = re.search(b"[a-zA-Z0-9_ -]+$", response_bytes)
            if text_content:
                return text_content.group(0).decode("utf-8")
    except Exception:
        pass

    # Fallback: decode and strip non-printable
    try:
        decoded = response_bytes.decode("utf-8", errors="ignore")
        import string

        return "".join(filter(lambda x: x in string.printable, decoded)).strip()
    except Exception:
        return str(response_bytes)


def run_test(pic, canister_id, method, expected_substr):
    """Run a query test and verify response contains expected substring."""
    print(f"\n=== Testing {method} ===")
    payload = encode([])  # Empty args
    response_bytes = pic.query_call(canister_id, method, payload)

    print(f"Raw response: {response_bytes}")

    # Naive decoding for now (until proper candid decode is hooked up)
    # We check if the expected bytes are present in the raw response
    # This works because Candid strings are UTF-8 encoded in the byte stream
    expected_bytes = expected_substr.encode("utf-8")
    passed = expected_bytes in response_bytes

    if passed:
        print(f"✓ Test passed: Response contains '{expected_substr}'")
    else:
        print(f"✗ Test failed: Expected '{expected_substr}' not found in response")
        # Try to decode for better error message
        decoded = decode_candid_text(response_bytes)
        print(f"  Decoded (approx): {decoded}")
        sys.exit(1)


def main():
    print("Starting PocketIC test for greet canister...")
    setup_pocketic_binary()

    wasm_path = get_wasm_path()
    did_path = get_did_path()

    import os
    import datetime

    print(f"WASM: {wasm_path}")
    print(
        f"      (Modified: {datetime.datetime.fromtimestamp(os.path.getmtime(wasm_path))})"
    )
    print(f"DID:  {did_path}")

    pic = PocketIC()
    print("PocketIC initialized")

    with open(wasm_path, "rb") as f:
        wasm_module = f.read()
    with open(did_path, "r") as f:
        candid_interface = f.read()

    print("Installing canister...")
    canister = pic.create_and_install_canister_with_candid(
        candid=candid_interface, wasm_module=wasm_module
    )
    canister_id = canister.canister_id
    print(f"Canister ID: {canister_id}")

    # Test 1: greet_no_arg
    run_test(pic, canister_id, "greet_no_arg", "hello world from cdk-c")

    # Test 2: greet (returns caller principal as text)
    # Default PocketIC anonymous caller is anoymous principal 
    # from IC-candid-spec anonymous principal: Binary: 0x04 ;text: 2vxsx-fae
    run_test(pic, canister_id, "greet_caller", "2vxsx-fae")

    print("\n=== All tests passed ===")


if __name__ == "__main__":
    main()
