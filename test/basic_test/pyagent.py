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
CID = "uzt4z-lp777-77774-qaabq-cai"
# Local IC network URL (default for dfx start)
URL = "http://127.0.0.1:4943"

# Initialize client, identity, and agent
client = Client(url=URL)
# Create a new anonymous identity
# To use dfx identity instead, read from ~/.config/dfx/identity/default/identity.pem
iden = Identity()
agent = Agent(iden, client)

# 1) Query call: greet_no_arg
# agent.query_raw automatically decodes Candid response
resp = agent.query_raw(CID, "greet_no_arg", encode([]))
print("greet_no_arg:", resp)  # Expected: ["hello world from cdk-c !"]

# 2) Query call: whoami
# Returns the principal of the caller
resp = agent.query_raw(CID, "whoami", encode([]))
print("whoami:", resp)  # Expected: caller's principal as string

# 3) Update call: trigger_call (requires principal parameter)
# Update calls can modify state and are executed on-chain
params = [{"type": Types.Principal, "value": CID}]  # Using self as target
resp = agent.update_raw(CID, "trigger_call", encode(params))
print("trigger_call:", resp)  # Usually [] when no return value
