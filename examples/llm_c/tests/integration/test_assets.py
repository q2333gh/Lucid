"""Integration tests for asset management."""

from __future__ import annotations

import os
import pathlib
import sys

import pytest

SCRIPT_DIR = pathlib.Path(__file__).parent.parent.parent
sys.path.insert(0, str(SCRIPT_DIR))

from tests.test_llm_c import (
    asset_exists,
    checkpoint_complete,
    load_asset,
    pick_checkpoint_path,
    query_canister_id,
    reset_assets,
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
    """Setup agent and canister ID."""
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
    return agent, canister_id


def test_asset_exists_query(agent_and_canister):
    """Test asset_exists query."""
    agent, canister_id = agent_and_canister
    # Test with non-existent asset
    assert not asset_exists(agent, canister_id, "nonexistent")
    # Test with existing asset (if any)
    # This will depend on what's already uploaded


def test_checkpoint_complete_query(agent_and_canister):
    """Test checkpoint_complete query."""
    agent, canister_id = agent_and_canister
    result = checkpoint_complete(agent, canister_id)
    assert isinstance(result, bool)


def test_reset_assets(agent_and_canister):
    """Test reset_assets update."""
    agent, canister_id = agent_and_canister
    reset_assets(agent, canister_id)
    # After reset, assets should not exist
    assert not asset_exists(agent, canister_id, "checkpoint")


def test_upload_assets_incremental(agent_and_canister):
    """Test incremental asset upload."""
    agent, canister_id = agent_and_canister
    
    # Reset first
    reset_assets(agent, canister_id)
    
    # Upload assets
    checkpoint_path = pick_checkpoint_path(None)
    upload_assets(agent, canister_id, checkpoint_path, None, skip_existing=True)
    
    # Verify assets loaded
    verify_assets_loaded(agent, canister_id)
    
    # Try uploading again with skip_existing=True (should skip)
    upload_assets(agent, canister_id, checkpoint_path, None, skip_existing=True)
    
    # Assets should still be there
    assert asset_exists(agent, canister_id, "checkpoint")
    assert checkpoint_complete(agent, canister_id)
