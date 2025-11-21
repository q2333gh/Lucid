# Testing Guide

## Quick Start

```bash
# Build first
python scripts/build.py --wasi --examples

# Run tests
python examples/hello_lucid/test/t1_candid.py
```

## Test Framework

Uses **PocketIC** for deterministic local testing.

## Test Structure

- Test file: `examples/hello_lucid/test/t1_candid.py`
- Uses Candid interface for type-safe testing
- Automatically sets up canister and verifies responses

## Prerequisites

```bash
pip install pocket-ic
```

Download PocketIC binary to `examples/hello_lucid/test/pocket-ic`

## example output result:
```
(wasm-build-script) jwk@jwk-vm ~/c/L/cdk-c (master)> python ./examples/hello_luci
d/test/t1_candid.py 
Starting PocketIC test for greet canister (using Candid interface)...
Set POCKET_IC_BIN to: /home/jwk/code/Lucid/cdk-c/examples/hello_lucid/test/pocket-ic
WASM file: /home/jwk/code/Lucid/cdk-c/build/greet_optimized.wasm
DID file: /home/jwk/code/Lucid/cdk-c/examples/hello_lucid/greet.did
2025-11-20T09:02:17.173999Z  INFO pocket_ic_server: The PocketIC server is listening on port 35859
PocketIC initialized
Creating and installing canister with Candid interface...
(This method automatically handles type parsing using the DID file)
Canister created and installed: lxzze-o7777-77777-aaaaa-cai
Canister object type: <class 'ic.canister.Canister'>

=== Testing greet_no_arg ===
2021-05-06 19:17:10.000000003 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] 
--
2021-05-06 19:17:10.000000003 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] IC_API caller's principal: aq
2021-05-06 19:17:10.000000003 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] debug print: hello dfx console 
2021-05-06 19:17:10.000000003 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] stdio print : hello dfx console
2021-05-06 19:17:10.000000003 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] 

Response: hello world from cdk-c
Response type: <class 'str'>
✓ greet_no_arg test passed

=== All tests completed ===

Benefits of using create_and_install_canister_with_candid:
1. Automatic canister setup - creates canister, adds cycles, installs code
2. Candid interface registration - DID file is parsed and stored
3. Simplified workflow - single method call instead of multiple steps
4. Canister object - provides canister_id and Candid interface reference

Note: For method calls, we use pic.query_call() and decode responses
      based on the DID file definition. The canister object's methods
      may have method name formatting issues, so direct query_call is used.
```