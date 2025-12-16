 Step 3/4: candid-extractor (interface extraction)
  Generated: examples/inter-canister-call/inter-canister-call.did
 Step 4/4: Copy optimized WASM to examples
   Copied inter-canister-call_ic.wasm -> /home/jwk/code/Lucid/examples/inter-canister-call/inter-canister-call_ic.wasm

 Summary: 1 converted, 1 optimized, 1 .did extracted
 Generating dfx.json configurations...
 Generating dfx.json for inter-canister-call in inter-canister-call...
Successfully generated /home/jwk/code/Lucid/examples/inter-canister-call/dfx.json

✓ Build complete.
2025-12-16T13:46:59.842935Z  INFO pocket_ic_server: The PocketIC server is listening on port 35123
[install-multi] PocketIC initialized
[install-multi] Installing 'adder'
[install-multi]  -> adder ID: lxzze-o7777-77777-aaaaa-cai
[install-multi] Installing 'inter-canister-call'
[install-multi]  -> inter-canister-call ID: lqy7q-dh777-77777-aaaaq-cai
[test_trigger] calling trigger_call on lqy7q-dh777-77777-aaaaq-cai with arg_text='lxzze-o7777-77777-aaaaa-cai,increment' (adder_id=lxzze-o7777-77777-aaaaa-cai)
2021-05-06 19:17:10.000000006 UTC: [Canister lqy7q-dh777-77777-aaaaq-cai] 
--
2021-05-06 19:17:10.000000006 UTC: [Canister lqy7q-dh777-77777-aaaaq-cai] ������
2021-05-06 19:17:10.000000006 UTC: [Canister lqy7q-dh777-77777-aaaaq-cai] trigger_call called
2021-05-06 19:17:10.000000006 UTC: [Canister lqy7q-dh777-77777-aaaaq-cai] Parsed callee for trigger_call: 
2021-05-06 19:17:10.000000006 UTC: [Canister lqy7q-dh777-77777-aaaaq-cai] lxzze-o7777-77777-aaaaa-cai
2021-05-06 19:17:10.000000006 UTC: [Canister lqy7q-dh777-77777-aaaaq-cai] Parsed method for trigger_call: 
2021-05-06 19:17:10.000000006 UTC: [Canister lqy7q-dh777-77777-aaaaq-cai] increment
2021-05-06 19:17:10.000000006 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] 
--
2021-05-06 19:17:10.000000006 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] ������
2021-05-06 19:17:10.000000006 UTC: [Canister lxzze-o7777-77777-aaaaa-cai] increment called ,result is : 1
2021-05-06 19:17:10.000000006 UTC: [Canister lqy7q-dh777-77777-aaaaq-cai] 
--
2021-05-06 19:17:10.000000006 UTC: [Canister lqy7q-dh777-77777-aaaaq-cai] ������
2021-05-06 19:17:10.000000006 UTC: [Canister lqy7q-dh777-77777-aaaaq-cai] Call replied! Now replying to original caller.
=== test_trigger: done, decoded='Inter-canister call successful.' ===