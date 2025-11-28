#!/usr/bin/env python3
"""
compare.py - Small helper to compare C implementation with `didc` oracle,
implemented using the `sh` library instead of raw subprocess.

Two modes:

  1) Encode: read Candid text from stdin, run both C encoder and `didc encode`,
     compare hex outputs.

  echo '("hello", 42)' | python3 oracle/compare.py encode \
        --c-bin build/bin/c_candid_encode

     C encoder contract: read Candid text from stdin, write hex (no 0x prefix)
     to stdout.

  2) Decode: read hex from stdin, run both C decoder and `didc decode`,
     compare Candid text outputs.

  echo "$HEX" | python3 oracle/compare.py decode \
        --c-bin build/bin/c_candid_decode

     C decoder contract: read hex from stdin, write Candid text to stdout.

By default this looks for `didc` on PATH; you can override via --didc-bin.
"""

import argparse
import os
import sys
import sh


def run_prog(prog: str, args, data: str) -> str:
    cmd = sh.Command(prog)
    out = cmd(*args, _in=data, _err=sys.stderr)
    return str(out).strip()


def main() -> None:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode", required=True)

    p_enc = sub.add_parser("encode", help="compare C encode vs didc encode")
    p_enc.add_argument(
        "--c-bin",
        default="build/bin/c_candid_encode",
    )
    p_enc.add_argument("--didc-bin", default=os.environ.get("DIDC_BIN", "didc"))

    p_dec = sub.add_parser("decode", help="compare C decode vs didc decode")
    p_dec.add_argument(
        "--c-bin",
        default="build/bin/c_candid_decode",
    )
    p_dec.add_argument("--didc-bin", default=os.environ.get("DIDC_BIN", "didc"))

    args, extra = parser.parse_known_args()
    data = sys.stdin.read()

    if args.mode == "encode":
        # C path
        c_out = run_prog(args.c_bin, extra, data)
        # Rust oracle path
        didc_out = run_prog(args.didc_bin, ["encode", "--format", "hex", *extra], data)
        if c_out == didc_out:
            print("OK: encode outputs match")
            return
        sys.stderr.write("Mismatch in encode outputs.\n")
        sys.stderr.write(f"C     : {c_out}\n")
        sys.stderr.write(f"didc  : {didc_out}\n")
        sys.exit(1)

    if args.mode == "decode":
        # C path
        c_out = run_prog(args.c_bin, extra, data)
        # Rust oracle path
        didc_out = run_prog(args.didc_bin, ["decode", "--format", "hex", *extra], data)
        if c_out == didc_out:
            print("OK: decode outputs match")
            return
        sys.stderr.write("Mismatch in decode outputs.\n")
        sys.stderr.write(f"C     : {c_out}\n")
        sys.stderr.write(f"didc  : {didc_out}\n")
        sys.exit(1)


if __name__ == "__main__":
    main()

