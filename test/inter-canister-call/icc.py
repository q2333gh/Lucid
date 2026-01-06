"""
Python agent example for interacting with IC canisters.

Prerequisites:
1. Start the local IC network: dfx start
2. Script will run `dfx deploy` in `examples/adder` and `examples/inter-canister-call`
3. Get the canister ID from dfx deploy output and update CID below
"""

from pathlib import Path
import os
import subprocess

from ic.client import Client
from ic.identity import Identity
from ic.agent import Agent
from ic.candid import encode, Types


def run_cmd(command, workdir: Path) -> None:
    """Run a command in the given directory and print stdout/stderr."""
    result = subprocess.run(
        command,
        cwd=workdir,
        text=True,
        capture_output=True,
        check=False,
        env={**os.environ, "DFX_DISABLE_COLOR": "1"},
    )
    if result.stdout:
        print(result.stdout.strip())
    if result.stderr:
        print(result.stderr.strip())
    if result.returncode != 0:
        raise subprocess.CalledProcessError(
            result.returncode, command, result.stdout, result.stderr
        )


def build_examples() -> None:
    """Build example canisters before deployment."""
    project_root = Path(__file__).resolve().parents[2]
    for example in ("adder", "inter-canister-call"):
        print(f"Building example {example} ...")
        run_cmd(
            ["python", "build.py", "--icwasm", "--examples", example],
            project_root,
        )


def deploy_examples() -> None:
    """Deploy example canisters required for the inter-canister call test."""
    project_root = Path(__file__).resolve().parents[2]
    for example in ("adder", "inter-canister-call"):
        example_dir = project_root / "examples" / example
        print(f"Deploying example in {example_dir} ...")
        run_cmd(["dfx", "deploy"], example_dir)


def main() -> None:
    # Avoid proxies interfering with local replica calls.
    for proxy_var in (
        "HTTP_PROXY",
        "http_proxy",
        "HTTPS_PROXY",
        "https_proxy",
        "ALL_PROXY",
        "all_proxy",
    ):
        os.environ.pop(proxy_var, None)

    build_examples()
    deploy_examples()

    # Canister ID - update this with your deployed canister IDs from 'dfx deploy' output
    # TODO dev-exp: find a way to pass a name instead of generated canister ID, a tool to KV store it .
    callee = "vt46d-j7777-77774-qaagq-cai"  # adder canister
    caller = "v56tl-sp777-77774-qaahq-cai"  # inter-canister-call canister
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
    payload = f"{callee},increment"
    params = [{"type": Types.Text, "value": payload}]
    resp = agent.update_raw(caller, "trigger_call", encode(params))
    print("trigger_call:", resp)  # Usually [] when no return value


if __name__ == "__main__":
    main()
