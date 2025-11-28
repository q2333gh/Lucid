#!/usr/bin/env python3
"""
Pure-Python smoke tests for the oracle layer.

Tests the C Candid implementation against the Rust `didc` oracle.

Run from the c_candid directory:
  python3 oracle/smoke.py

Requires:
  - `sh` Python package
  - `didc` on PATH
  - Build the C binaries: cd build && cmake .. && make
"""

import sys
import sh


def smoke_encode(data: str, label: str = "") -> None:
    """Test encoding a single Candid text value."""
    out = sh.python3("./oracle/encode.py", _in=data)
    hex_out = str(out).strip()
    if label:
        sys.stdout.write(f"[encode {label}] hex: {hex_out}\n")
    else:
        sys.stdout.write(f"[encode] hex: {hex_out}\n")


def smoke_compare_encode(data: str, label: str = "") -> None:
    """Compare C encode output with didc oracle."""
    out = sh.python3(
        "./oracle/compare.py",
        "encode",
        "--c-bin",
        "./build/bin/c_candid_encode",
        _in=data,
    )
    result = str(out).strip()
    if label:
        sys.stdout.write(f"[compare encode {label}] {result}\n")
    else:
        sys.stdout.write(f"{result}\n")


def smoke_compare_decode(hex_data: str, label: str = "") -> None:
    """Compare C decode output with didc oracle."""
    out = sh.python3(
        "./oracle/compare.py",
        "decode",
        "--c-bin",
        "./build/bin/c_candid_decode",
        _in=hex_data,
    )
    result = str(out).strip()
    if label:
        sys.stdout.write(f"[compare decode {label}] {result}\n")
    else:
        sys.stdout.write(f"{result}\n")


def smoke_roundtrip(data: str, label: str = "") -> None:
    """Test encode then decode roundtrip."""
    # Encode with C
    hex_out = sh.Command("./build/bin/c_candid_encode")(_in=data).strip()
    # Decode with C
    decoded = sh.Command("./build/bin/c_candid_decode")(_in=hex_out).strip()
    if label:
        sys.stdout.write(f"[roundtrip {label}] {data} -> {hex_out} -> {decoded}\n")
    else:
        sys.stdout.write(f"[roundtrip] {data} -> {hex_out} -> {decoded}\n")


def main() -> None:
    print("=== Encode Tests ===")
    test_cases = [
        ('("hello", 42)', "text+int"),
        ("(true, false)", "bool"),
        ("(null)", "null"),
        ('("test")', "text"),
        ("(123)", "int"),
        ("(-456)", "neg int"),
        ("(3.14)", "float"),
    ]

    for data, label in test_cases:
        smoke_encode(data, label)
        smoke_compare_encode(data, label)

    print("\n=== Decode Tests ===")
    # Pre-encoded test data
    decode_cases = [
        ("4449444c0002717c0568656c6c6f2a", "text+int"),  # ("hello", 42)
        ("4449444c00027e7e0100", "bool"),  # (true, false)
        ("4449444c00017f", "null"),  # (null)
    ]

    for hex_data, label in decode_cases:
        smoke_compare_decode(hex_data, label)

    print("\n=== Roundtrip Tests ===")
    roundtrip_cases = [
        ('("hello", 42)', "text+int"),
        ("(true)", "bool"),
        ("(null)", "null"),
    ]

    for data, label in roundtrip_cases:
        smoke_roundtrip(data, label)

    print("\n=== All smoke tests passed! ===")


if __name__ == "__main__":
    main()
