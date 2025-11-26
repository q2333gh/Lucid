#!/usr/bin/env python3
"""
Pure-Python smoke tests for the oracle layer.

Intended to replace shell one-liners like:

  echo '("hello", 42)' | python3 c/oracle/encode.py
  echo '("hello", 42)' | python3 c/oracle/compare.py encode --c-bin ./c_candid_encode

You can simply run:

  python3 c/oracle/smoke.py

from the repository root. Requires the `sh` package and a working `didc`.
"""

import sys
import sh


def smoke_encode() -> None:
    data = '("hello", 42)'
    out = sh.python3("c/oracle/encode.py", _in=data)
    sys.stdout.write("[encode] hex: " + str(out).strip() + "\n")


def smoke_compare_encode() -> None:
    data = '("hello", 42)'
    out = sh.python3(
        "c/oracle/compare.py",
        "encode",
        "--c-bin",
        "./c_candid_encode",
        _in=data,
    )
    # compare.py already prints OK / mismatch info
    sys.stdout.write(str(out))


def main() -> None:
    smoke_encode()
    smoke_compare_encode()


if __name__ == "__main__":
    main()


