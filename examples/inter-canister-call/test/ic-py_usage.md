Modules & Usage
1. Principal
Create an instance:

from ic.principal import Principal

p  = Principal()                 # default is management canister id `aaaaa-aa`
p1 = Principal(bytes=b'')        # from bytes
p2 = Principal.anonymous()       # anonymous principal
p3 = Principal.self_authenticating(pubkey)  # from public key
p4 = Principal.from_str('aaaaa-aa')         # from string
p5 = Principal.from_hex('deadbeef')         # from hex
Properties & methods:

p.bytes   # principal bytes
p.len     # byte array length
p.to_str()# convert to string
2. Identity
Create an instance:

from ic.identity import Identity

i  = Identity()  # randomly generated key
i1 = Identity(privkey="833fe62409237b9d62ec77587520911e9a759cec1d19755b7da901b96dca3d42")
Sign a message:

msg = bytes.fromhex(
    "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
    "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"
)
der_pubkey, signature = i.sign(msg)  # tuple: (der_encoded_pubkey, signature)
3. Client
from ic.client import Client
client = Client(url="https://ic0.app")
4. Candid
Encode parameters:

from ic.candid import encode, decode, Types

# params is a list, returns encoded bytes
params = [{'type': Types.Nat, 'value': 10}]
data = encode(params)
Decode parameters:

# data is bytes, returns a parameter list
params = decode(data)
5. Agent
Create an instance:

from ic.client import Client
from ic.identity import Identity
from ic.agent import Agent

iden = Identity()
client = Client()
agent = Agent(iden, client)
Query call:

# query the name of token canister `gvbup-jyaaa-aaaah-qcdwa-cai`
from ic.candid import encode
name = agent.query_raw("gvbup-jyaaa-aaaah-qcdwa-cai", "name", encode([]))
Update call:

# transfer 100 tokens to blackhole address `aaaaa-aa`
from ic.candid import Types, encode

params = [
  {'type': Types.Principal, 'value': 'aaaaa-aa'},
  {'type': Types.Nat,       'value': 10000000000}
]
result = agent.update_raw(
  "gvbup-jyaaa-aaaah-qcdwa-cai",
  "transfer",
  encode(params),
  # verify_certificate=True,  # enable if you installed `blst`
)
6. Canister
Create a canister instance with Candid interface file and canister id, and call a method:

from ic.canister import Canister
from ic.client import Client
from ic.identity import Identity
from ic.agent import Agent

iden = Identity()
client = Client()
agent = Agent(iden, client)

# read governance candid from file
governance_did = open("governance.did").read()

# create a governance canister instance
governance = Canister(agent=agent,
                      canister_id="rrkah-fqaaa-aaaaa-aaaaq-cai",
                      candid=governance_did)

# call canister method with instance
res = governance.list_proposals({
    'include_reward_status': [],
    'before_proposal': [],
    'limit': 100,
    'exclude_topic': [],
    'include_status': [1],
})
7. Async request
ic-py also supports async requests:

import asyncio
from ic.canister import Canister
from ic.client import Client
from ic.identity import Identity
from ic.agent import Agent

iden = Identity()
client = Client()
agent = Agent(iden, client)

governance_did = open("governance.did").read()
governance = Canister(agent=agent,
                      canister_id="rrkah-fqaaa-aaaaa-aaaaq-cai",
                      candid=governance_did)

async def async_test():
    res = await governance.list_proposals_async({
        'include_reward_status': [],
        'before_proposal': [],
        'limit': 100,
        'exclude_topic': [],
        'include_status': [1],
    })
    print(res)

asyncio.run(async_test())