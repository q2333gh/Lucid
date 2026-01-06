"""
Python agent example for testing HTTP request canister.

Prerequisites:
1. Start the local IC network: dfx start
2. Script will build and deploy http_request example
3. Test the HTTP GET functionality
"""

from pathlib import Path
import os
import subprocess

from ic.client import Client
from ic.identity import Identity
from ic.agent import Agent
from ic.candid import decode


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


def build_http_request() -> None:
    """Build http_request canister before deployment."""
    project_root = Path(__file__).resolve().parents[2]
    print("Building http_request example ...")
    run_cmd(
        ["python", "build.py", "--icwasm", "--examples", "http_request"],
        project_root,
    )


def deploy_http_request() -> Path:
    """Deploy http_request canister and return example directory."""
    project_root = Path(__file__).resolve().parents[2]
    example_dir = project_root / "examples" / "http_request"
    print(f"Deploying http_request in {example_dir} ...")
    run_cmd(["dfx", "deploy", "--yes"], example_dir)
    return example_dir


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

    build_http_request()
    example_dir = deploy_http_request()

    # Test via dfx canister call
    print("\n=== Testing http_get_simple (first call - initiates request) ===")
    run_cmd(["dfx", "canister", "call", "http_request", "http_get_simple"], example_dir)

    # print("\n=== Waiting a moment for HTTP request to complete ===")
    # import time

    # time.sleep(3)

    # print("\n=== Testing http_get_simple (second call - retrieves response) ===")
    # run_cmd(["dfx", "canister", "call", "http_request", "http_get_simple"], example_dir)


if __name__ == "__main__":
    main()
