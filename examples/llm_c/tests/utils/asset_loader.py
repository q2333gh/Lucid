"""Reusable asset loading utilities for tests."""

from __future__ import annotations

import pathlib
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from ic.agent import Agent


def load_asset_chunk(
    agent: Agent,
    canister_id: str,
    name: str,
    chunk: bytes,
    chunk_index: int,
) -> None:
    """Load a single chunk of an asset."""
    from ic.candid import encode, Types

    args = [
        {"type": Types.Text, "value": name},
        {"type": Types.Vec(Types.Nat8), "value": list(chunk)},
    ]
    agent.update_raw(canister_id, "load_asset", encode(args))
    print(f"...{name}: sent chunk {chunk_index} ({len(chunk)} bytes)")


def load_asset_file(
    agent: Agent,
    canister_id: str,
    name: str,
    path: pathlib.Path,
    chunk_size: int = 1_900_000,
) -> int:
    """Load an asset file in chunks.
    
    Returns:
        Total bytes loaded
    """
    if not path.exists():
        raise FileNotFoundError(f"{path} does not exist")
    
    print(f"Loading asset {name} from {path}...")
    chunk_index = 0
    total_bytes = 0
    
    with path.open("rb") as asset_file:
        while True:
            chunk = asset_file.read(chunk_size)
            if not chunk:
                break
            chunk_index += 1
            load_asset_chunk(agent, canister_id, name, chunk, chunk_index)
            total_bytes += len(chunk)
    
    print(f"...{name} loaded ({total_bytes} bytes in {chunk_index} chunks)")
    return total_bytes
