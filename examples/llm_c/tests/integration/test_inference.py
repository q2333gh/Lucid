"""Integration tests for inference operations."""

from __future__ import annotations

import os
import pathlib
import sys

import pytest

SCRIPT_DIR = pathlib.Path(__file__).parent.parent.parent
sys.path.insert(0, str(SCRIPT_DIR))

from tests.test_llm_c import (
    call_infer,
    pick_checkpoint_path,
    query_canister_id,
    upload_assets,
    verify_assets_loaded,
    wait_for_replica,
)

pytestmark = pytest.mark.skipif(
    not os.environ.get("LLM_TEST_ENABLE_INTEGRATION"),
    reason="Integration tests disabled (set LLM_TEST_ENABLE_INTEGRATION=1)",
)


@pytest.fixture(scope="module")
def agent_and_canister():
    """Setup agent and canister ID with assets loaded."""
    from ic.agent import Agent
    from ic.client import Client
    from ic.identity import Identity

    # Unset proxies
    for proxy_var in ("HTTP_PROXY", "http_proxy", "HTTPS_PROXY", "https_proxy", "ALL_PROXY", "all_proxy"):
        os.environ.pop(proxy_var, None)

    wait_for_replica()
    canister_id = query_canister_id()
    identity = Identity()
    client = Client(url=os.environ.get("LLM_TEST_REPLICA", "http://127.0.0.1:4943"))
    agent = Agent(identity, client)
    
    # Ensure assets are loaded
    checkpoint_path = pick_checkpoint_path(None)
    upload_assets(agent, canister_id, checkpoint_path, None, skip_existing=True)
    verify_assets_loaded(agent, canister_id)
    
    return agent, canister_id


def test_infer_hello_update(agent_and_canister):
    """Test infer_hello_update."""
    agent, canister_id = agent_and_canister
    result = call_infer(agent, canister_id, None, False, 0, 64, False)
    assert isinstance(result, str)
    assert len(result) > 0


def test_infer_hello_update_limited(agent_and_canister):
    """Test infer_hello_update_limited."""
    agent, canister_id = agent_and_canister
    result = call_infer(agent, canister_id, 5, False, 0, 64, False)
    assert isinstance(result, str)
    assert len(result) > 0


def test_infer_stories_update(agent_and_canister):
    """Test infer_stories_update (if stories asset available)."""
    agent, canister_id = agent_and_canister
    stories_path = SCRIPT_DIR / "assets" / "stories15M.bin"
    if stories_path.exists():
        # upload_assets already imported above
        upload_assets(agent, canister_id, None, stories_path, skip_existing=True)
        result = call_infer(agent, canister_id, 1, True, 0, 64, False)
        assert isinstance(result, str)
        assert len(result) > 0
