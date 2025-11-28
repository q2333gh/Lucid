## Oracle baseline (Phase 0-A)

This directory contains Rust `didc`-based **reference tools** used as an oracle
when developing and testing the C implementation of Candid.

### Prerequisites
isntall didc cli tool
```bash
cargo binsintall   didc 
```

### Tools

- `encode.py`  
  - stdin: Candid text (for example `("hello", 42)`).  
  - stdout: Candid binary encoded as a hex string (no `0x` prefix).  
  - Internally calls: `didc encode --format hex …`.

- `decode.py`  
  - stdin: Candid binary as a hex string.  
  - stdout: Candid text.  
  - Internally calls: `didc decode --format hex …`.

- `compare.py`  
  - Compares the **C implementation** against `didc` for both encode and decode.  
  - Assumes the stub binaries built via `python3 c_candid/build.py bins`:
    - `build/bin/c_candid_encode`: reads Candid text from stdin, writes hex to stdout.  
    - `build/bin/c_candid_decode`: reads hex from stdin, writes Candid text to stdout.

#### Example: compare encode

```bash
echo '("hello", 42)' | python3 oracle/compare.py encode \
  --c-bin build/bin/c_candid_encode \
  --didc-bin "${DIDC_BIN:-didc}"
```

#### Example: compare decode

```bash
echo "$HEX" | python3 oracle/compare.py decode \
  --c-bin build/bin/c_candid_decode \
  --didc-bin "${DIDC_BIN:-didc}"
```

If the outputs match, `compare.py` prints `OK: encode/decode outputs match`.
Otherwise it prints both sides and exits with a non-zero status code, so it can
be wired directly into CI.
