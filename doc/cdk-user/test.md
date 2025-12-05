# Testing Guide

## Quick Start

```bash
# Build first (from project root)
python build.py --icwasm

# Run tests
python examples/hello_lucid/test/t1_candid.py
```

## Test Framework

This project supports two testing approaches:

1. **PocketIC** - Fast, deterministic local testing (recommended for CI/CD)
2. **IC Local Replica** - Full IC replica simulation for integration testing

## Test Structure

- Test file: `examples/hello_lucid/test/t1_candid.py`
- Uses Candid interface for type-safe testing
- Automatically sets up canister and verifies responses

## Prerequisites

```bash
pip install pocket-ic
```

Download PocketIC binary to `examples/hello_lucid/test/pocket-ic`

## Example Output

```
jwk@jwk-vm ~/c/Lucid (master)> python examples/hello_lucid/test/t1_candid.py
Starting PocketIC test for greet canister...
Set POCKET_IC_BIN to: /home/jwk/code/Lucid/examples/hello_lucid/test/pocket-ic
WASM: /home/jwk/code/Lucid/build-wasi/bin/hello_lucid_ic.wasm
      (Modified: 2025-12-06 02:17:12.385765)
DID:  /home/jwk/code/Lucid/examples/hello_lucid/hello_lucid.did
2025-12-05T18:17:23.420523Z  INFO pocket_ic_server: The PocketIC server is listening on port 36799
PocketIC initialized
Installing canister...
Canister ID: lxzze-o7777-77777-aaaaa-cai

=== Testing greet_no_arg ===
2021-05-06 19:17:10.000000003 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] 
--
2021-05-06 19:17:10.000000003 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] debug print: hello dfx console. 
Raw response: b'DIDL\x00\x01q\x18hello world from cdk-c !'
✓ Test passed: Response contains 'hello world from cdk-c'

=== Testing greet_caller ===
2021-05-06 19:17:10.000000003 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] 
--
2021-05-06 19:17:10.000000003 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] caller: 2vxsx-fae
Raw response: b'DIDL\x00\x01q\t2vxsx-fae'
✓ Test passed: Response contains '2vxsx-fae'

=== All tests passed ===
```

## Test Details

The test script uses PocketIC's `create_and_install_canister_with_candid` method which provides:

1. **Automatic canister setup** - Creates canister, adds cycles, installs code
2. **Candid interface registration** - DID file is parsed and stored automatically
3. **Simplified workflow** - Single method call instead of multiple steps
4. **Type-safe testing** - Uses Candid interface for encoding/decoding

The test verifies:
- `greet_no_arg`: Returns a simple greeting message
- `greet_caller`: Returns the caller's principal as text (anonymous principal `2vxsx-fae`)

## Testing with DFX Local Replica

For a more complete IC simulation, you can use DFX's local replica. This provides a full IC environment that closely matches the production network.

### Prerequisites

Install DFX (DFINITY Canister SDK):

```bash
# Install DFX (if not already installed)
sh -ci "$(curl -fsSL https://sdk.dfinity.org/install.sh)"
```

### Setup and Deployment

1. **Start the local replica** (in a separate terminal):

```bash
dfx start
```

The replica will start and display:
```
Running dfx start for version 0.29.2
Using the default configuration for the local shared network.
Replica API running on 127.0.0.1:4943. You must open a new terminal to continue developing.
```

2. **Navigate to your example directory** and deploy:

```bash
cd examples/hello_lucid
dfx deploy
```

This will:
- Create the canister on the local replica
- Build and install the WASM code
- Register the Candid interface
- Display the canister ID and Candid UI URL

Example output:
```
Deploying all canisters.
hello_lucid canister created with canister id: uzt4z-lp777-77774-qaabq-cai
Building canister 'hello_lucid'.
Installed code for canister hello_lucid, with canister ID uzt4z-lp777-77774-qaabq-cai
Deployed canisters.
URLs:
  Backend canister via Candid interface:
    hello_lucid: http://127.0.0.1:4943/?canisterId=u6s2n-gx777-77774-qaaba-cai&id=uzt4z-lp777-77774-qaabq-cai
```

### Calling Canister Methods

Use `dfx canister call` to invoke canister methods:

```bash
# Query method (no state change)
dfx canister call hello_lucid greet_no_arg --query

# Query method that returns caller principal
dfx canister call hello_lucid greet_caller --query
```

Example output:
```
("gsgya-23caw-qaptp-sngp3-z3xt3-jdulq-vaojq-meflh-tb2y6-njn5e-yae")
```

### Using the Candid UI

The deployment output includes a Candid UI URL. Open it in your browser to:
- View all available methods
- Call methods interactively
- Inspect method signatures and return values
- Test with different input parameters

### Stopping the Replica

To stop the local replica, press `Ctrl-C` in the terminal where `dfx start` is running, or run:

```bash
dfx stop
```

### Advantages of DFX Local Replica

- **Full IC environment** - Closest match to production behavior
- **Candid UI** - Interactive testing interface
- **Realistic networking** - Simulates actual IC network calls
- **State persistence** - Canister state persists across restarts (in `.dfx/local`)
- **Multiple canisters** - Can deploy and test multiple canisters together

### Comparison: PocketIC vs DFX Replica

| Feature | PocketIC | DFX Replica |
|---------|----------|-------------|
| Speed | Very fast | Slower (full simulation) |
| Determinism | Fully deterministic | Deterministic |
| CI/CD | Excellent | Good |
| Candid UI | No | Yes |
| State persistence | In-memory only | Persistent |
| Network simulation | Minimal | Full |
| Best for | Unit tests, CI | Integration tests, manual testing |

## Tested Code Example

The canister functions being tested use the simplified API:

```c
IC_API_QUERY(greet_no_arg, "() -> (text)") {
    ic_api_debug_print("debug print: hello dfx console. ");
    IC_API_REPLY_TEXT("hello world from cdk-c !");
}

IC_API_QUERY(greet_caller, "() -> (text)") {
    ic_principal_t caller = ic_api_get_caller(api);
    char caller_text[64];
    ic_principal_to_text(&caller, caller_text, sizeof(caller_text));
    IC_API_REPLY_TEXT(caller_text);
}
```