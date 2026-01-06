#!/usr/bin/env python3
"""
Verify Candid encoding from C library using Python ic-py
"""
import os
import sys

# Clear proxies
for var in [
    "HTTP_PROXY",
    "http_proxy",
    "HTTPS_PROXY",
    "https_proxy",
    "ALL_PROXY",
    "all_proxy",
]:
    os.environ.pop(var, None)

from ic.candid import decode


def verify_file(filename):
    """Verify a Candid binary file"""
    print(f"=== Verifying {filename} ===")

    try:
        with open(filename, "rb") as f:
            data = f.read()

        print(f"File size: {len(data)} bytes")
        print(f"Hex: {data.hex()}")

        # Check DIDL magic
        if data[:4] != b"DIDL":
            print("✗ FAIL: Missing DIDL magic")
            return False

        print("✓ DIDL magic present")

        # Try to decode
        try:
            result = decode(data)
            print(f"✓ SUCCESS: Decoded {len(result)} value(s)")
            for i, val in enumerate(result):
                print(f"  Value {i}: {val}")
            return True
        except Exception as e:
            print(f"✗ FAIL: Decode error: {e}")
            return False

    except FileNotFoundError:
        print(f"✗ File not found: {filename}")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False


if __name__ == "__main__":
    files = [
        "simple_record.bin",
        "http_args_complete.bin",
    ]

    results = []
    for f in files:
        result = verify_file(f)
        results.append((f, result))
        print()

    print("=== Summary ===")
    for f, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"{status}: {f}")

    sys.exit(0 if all(r for _, r in results) else 1)
