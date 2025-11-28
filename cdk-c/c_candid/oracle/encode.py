#!/usr/bin/env python3
import os
import sys
import sh


def main() -> None:
    """
    Python wrapper around `didc encode --format hex`, implemented via `sh`.

    stdin : Candid textual values (e.g. `(\"hello\", 42)`).
    stdout: Hex string (no 0x prefix).

    DIDC_BIN env var can override the `didc` binary path.
    Extra CLI args are forwarded to `didc encode`.
    """
    didc_bin = os.environ.get("DIDC_BIN", "didc")
    data = sys.stdin.read()

    didc = sh.Command(didc_bin)
    out = didc("encode", "--format", "hex", *sys.argv[1:], _in=data, _err=sys.stderr)
    sys.stdout.write(str(out))


if __name__ == "__main__":
    main()

