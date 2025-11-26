#!/usr/bin/env python3
import os
import sys
import sh


def main() -> None:
    """
    Python wrapper around `didc decode --format hex`, implemented via `sh`.

    stdin : Hex string (no 0x prefix).
    stdout: Candid textual values.

    DIDC_BIN env var can override the `didc` binary path.
    Extra CLI args are forwarded to `didc decode`.
    """
    didc_bin = os.environ.get("DIDC_BIN", "didc")
    data = sys.stdin.read()

    didc = sh.Command(didc_bin)
    out = didc("decode", "--format", "hex", *sys.argv[1:], _in=data, _err=sys.stderr)
    sys.stdout.write(str(out))


if __name__ == "__main__":
    main()

