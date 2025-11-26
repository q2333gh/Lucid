## Oracle baseline (Phase 0-A)

This directory contains Rust `didc`-based **reference tools** used as an oracle
when developing and testing the C implementation of Candid.

### Prerequisites

- Build `didc` at the repository root:

```bash
cargo build -p didc --release
```

- Add `target/release/didc` to your `PATH`, or set an explicit binary via:

```bash
export DIDC_BIN=/abs/path/to/target/release/didc
```

### Tools

- `encode.sh`  
  - stdin: Candid text (for example `("hello", 42)`).  
  - stdout: Candid binary encoded as a hex string (no `0x` prefix).  
  - Internally calls: `didc encode --format hex …`.

- `decode.sh`  
  - stdin: Candid binary as a hex string.  
  - stdout: Candid text.  
  - Internally calls: `didc decode --format hex …`.

- `compare.py`  
  - Compares the **C implementation** against `didc` for both encode and decode.  
  - Assumes there are two C executables:
    - `./c_candid_encode`: reads Candid text from stdin, writes hex to stdout.  
    - `./c_candid_decode`: reads hex from stdin, writes Candid text to stdout.

#### Example: compare encode

```bash
echo '("hello", 42)' | ./oracle/compare.py encode \
  --c-bin ./c_candid_encode \
  --didc-bin "${DIDC_BIN:-didc}"
```

#### Example: compare decode

```bash
echo "$HEX" | ./oracle/compare.py decode \
  --c-bin ./c_candid_decode \
  --didc-bin "${DIDC_BIN:-didc}"
```

If the outputs match, `compare.py` prints `OK: encode/decode outputs match`.
Otherwise it prints both sides and exits with a non-zero status code, so it can
be wired directly into CI.
