"""
Python agent example for interacting with IC canisters.

Prerequisites:
1. Start the local IC network: dfx start
2. Deploy the canister: dfx deploy
3. Get the canister ID from dfx deploy output and update CID below
"""

from ic.client import Client
from ic.identity import Identity
from ic.agent import Agent
from ic.candid import encode, decode, Types

# Canister ID - update this with your deployed canister ID from 'dfx deploy' output
# TODO dev-exp: find a way to pass a name instead of generated canister ID, a tool to KV store it .
callee = "ufxgi-4p777-77774-qaadq-cai"
caller = "vpyes-67777-77774-qaaeq-cai"
# Default local IC network URL (default for dfx start)
ic_network_url = "http://127.0.0.1:4943"

# Initialize client, identity, and agent
client = Client(url=ic_network_url)
# Create a new anonymous identity
# To use dfx identity instead, read from ~/.config/dfx/identity/default/identity.pem
iden = Identity()
agent = Agent(iden, client)

print("inter-canister call increment:")
# 3) Update call: trigger_call (requires principal parameter)
# Update calls can modify state and are executed on-chain
params = [{"type": Types.Principal, "value": callee}]  # Using self as target
resp = agent.update_raw(caller, "trigger_call", encode(params))
print("trigger_call:", resp)  # Usually [] when no return value
