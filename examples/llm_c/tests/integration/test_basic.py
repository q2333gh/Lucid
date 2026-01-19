"""Basic integration tests for canister operations."""

from __future__ import annotations

import os
import pathlib
import subprocess
import sys
import time

import pytest

# Import test utilities
SCRIPT_DIR = pathlib.Path(__file__).parent.parent.parent
REPO_ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

# These tests require dfx and a running replica
pytestmark = pytest.mark.skipif(
    not os.environ.get("LLM_TEST_ENABLE_INTEGRATION"),
    reason="Integration tests disabled (set LLM_TEST_ENABLE_INTEGRATION=1)",
)


def wait_for_replica(max_attempts: int = 20, delay_s: float = 1.0) -> None:
    """Wait for the local replica to accept requests."""
    for attempt in range(1, max_attempts + 1):
        try:
            subprocess.run(
                ["dfx", "ping"],
                cwd=SCRIPT_DIR,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=True,
            )
            return
        except (subprocess.CalledProcessError, FileNotFoundError):
            if attempt < max_attempts:
                time.sleep(delay_s)
            else:
                pytest.skip("dfx replica not available")


def query_canister_id() -> str:
    """Read the local deployment's canister id for llm_c."""
    try:
        result = subprocess.run(
            ["dfx", "canister", "id", "llm_c"],
            cwd=SCRIPT_DIR,
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        pytest.skip("dfx not available or canister not deployed")


@pytest.fixture(scope="module")
def canister_id():
    """Get canister ID (requires deployed canister)."""
    wait_for_replica()
    return query_canister_id()


def test_hello_query(canister_id: str):
    """Test basic hello query."""
    from ic.agent import Agent
    from ic.client import Client
    from ic.identity import Identity
    from ic.candid import encode, Types

    # Unset proxies
    for proxy_var in ("HTTP_PROXY", "http_proxy", "HTTPS_PROXY", "https_proxy", "ALL_PROXY", "all_proxy"):
        os.environ.pop(proxy_var, None)

    identity = Identity()
    client = Client(url=os.environ.get("LLM_TEST_REPLICA", "http://127.0.0.1:4943"))
    agent = Agent(identity, client)

    encoded_args = encode([])
    result = agent.query_raw(canister_id, "hello", encoded_args, return_type=[Types.Text])
    assert isinstance(result, list)
    assert len(result) > 0
    assert "hello" in result[0].lower()
