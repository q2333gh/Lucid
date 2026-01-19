#!/usr/bin/env python3
from __future__ import annotations
import os
import subprocess
from ic.agent import Agent
from ic.client import Client
from ic.identity import Identity
from ic.candid import encode, Types

def main():
    for proxy in ("HTTP_PROXY","http_proxy","HTTPS_PROXY","https_proxy","ALL_PROXY","all_proxy"):
        os.environ.pop(proxy,None)
    canister_id = subprocess.run(["dfx","canister","id","llm_c"],cwd="examples/llm_c",text=True,capture_output=True,check=True).stdout.strip()
    agent = Agent(Identity(), Client(url="http://127.0.0.1:4943"))
    sizes = [8<<10, 32<<10, 64<<10, 80<<10, 128<<10, 256<<10, 512<<10, 1024<<10, 2*1024<<10, 3*1024<<10]
    for size in sizes:
        chunk = bytes([size % 256]) * size
        args = [{"type": Types.Text, "value": "chunk_probe"}, {"type": Types.Vec(Types.Nat8), "value": list(chunk)}]
        try:
            agent.update_raw(canister_id,"load_asset",encode(args))
            print(f"SUCCESS size={size} bytes")
        except Exception as exc:
            print(f"FAIL size={size} => {exc}")
            break

if __name__ == "__main__":
    main()
