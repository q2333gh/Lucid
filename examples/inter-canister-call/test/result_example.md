(daily_ic_dev) jwk@jwk-vm ~/c/Lucid (master)> python test/inter-canister-cal
l/icc.py
Building example adder ...
-- Building examples: adder
-- Applying WASM size optimizations...
-- Configuring done
-- Generating done
-- Build files have been written to: /home/jwk/code/Lucid/build-wasi
[1/2] Building C object examples/adder/CMakeFiles/adder.dir/greet.c.obj
[2/2] Linking C executable bin/adder.wasm
[Phase 1] Toolchain: WASI SDK
 SDK: /home/jwk/opt/wasi-sdk
 Toolchain: wasi-sdk.cmake

[Phase 2] IC Tools: polyfill + wasi2ic
 Polyfill: libic_wasi_polyfill.a
 wasi2ic: wasi2ic

[Phase 3] Build: CMake + Ninja
 Directory: /home/jwk/code/Lucid/build-wasi
 Configuring...
 Compiling...

[Phase 4] Post-processing: wasi2ic -> wasm-opt -> candid -> dfx.json
 Step 1/4: wasi2ic (WASI -> IC conversion)
   adder.wasm -> adder_ic.wasm
 Step 2/4: wasm-opt (binary optimization)
   adder_ic.wasm
 Optimized: 438,027 -> 435,915 bytes (0.5% reduction)
 Step 3/4: candid-extractor (interface extraction)
  Generated: examples/adder/adder.did
 Step 4/4: Copy optimized WASM to examples
   Copied adder_ic.wasm -> /home/jwk/code/Lucid/examples/adder/adder_ic.wasm

 Summary: 1 converted, 1 optimized, 1 .did extracted
 Generating dfx.json configurations...
 Generating dfx.json for adder in adder...
Successfully generated /home/jwk/code/Lucid/examples/adder/dfx.json

✓ Build complete.
Building example inter-canister-call ...
-- Building examples: inter-canister-call
-- Applying WASM size optimizations...
-- Configuring done
-- Generating done
-- Build files have been written to: /home/jwk/code/Lucid/build-wasi
[1/2] Building C object examples/inter-canister-call/CMakeFiles/inter-canister-call.dir/greet.c.obj
[2/2] Linking C executable bin/inter-canister-call.wasm
[Phase 1] Toolchain: WASI SDK
 SDK: /home/jwk/opt/wasi-sdk
 Toolchain: wasi-sdk.cmake

[Phase 2] IC Tools: polyfill + wasi2ic
 Polyfill: libic_wasi_polyfill.a
 wasi2ic: wasi2ic

[Phase 3] Build: CMake + Ninja
 Directory: /home/jwk/code/Lucid/build-wasi
 Configuring...
 Compiling...

[Phase 4] Post-processing: wasi2ic -> wasm-opt -> candid -> dfx.json
 Step 1/4: wasi2ic (WASI -> IC conversion)
   inter-canister-call.wasm -> inter-canister-call_ic.wasm
 Step 2/4: wasm-opt (binary optimization)
   inter-canister-call_ic.wasm
 Optimized: 448,536 -> 446,359 bytes (0.5% reduction)
 Step 3/4: candid-extractor (interface extraction)
  Generated: examples/inter-canister-call/inter-canister-call.did
 Step 4/4: Copy optimized WASM to examples
   Copied inter-canister-call_ic.wasm -> /home/jwk/code/Lucid/examples/inter-canister-call/inter-canister-call_ic.wasm

 Summary: 1 converted, 1 optimized, 1 .did extracted
 Generating dfx.json configurations...
 Generating dfx.json for inter-canister-call in inter-canister-call...
Successfully generated /home/jwk/code/Lucid/examples/inter-canister-call/dfx.json

✓ Build complete.
Deploying example in /home/jwk/code/Lucid/examples/adder ...
Deploying all canisters.
All canisters have already been created.
Building canister 'adder'.
Module hash c77b5da68ce17ed247a12a6d0d96876ec3b732190497ef02d229fdc6ece2ea8a is already installed.
Upgraded code for canister adder, with canister ID vt46d-j7777-77774-qaagq-cai
Deployed canisters.
URLs:
  Backend canister via Candid interface:
    adder: http://127.0.0.1:4943/?canisterId=v27v7-7x777-77774-qaaha-cai&id=vt46d-j7777-77774-qaagq-cai
Deploying example in /home/jwk/code/Lucid/examples/inter-canister-call ...
Deploying all canisters.
All canisters have already been created.
Building canister 'inter-canister-call'.
Module hash d7ce56047fc0f5c4dd6566dd6a495cc3c0d17ab0b97e30e50d7e9f3c71029220 is already installed.
Upgraded code for canister inter-canister-call, with canister ID v56tl-sp777-77774-qaahq-cai
Deployed canisters.
URLs:
  Backend canister via Candid interface:
    inter-canister-call: http://127.0.0.1:4943/?canisterId=xad5d-bh777-77774-qaaia-cai&id=v56tl-sp777-77774-qaahq-cai
inter-canister call increment:
trigger_call: [{'type': 'text', 'value': 'Incremented! value=4'}]