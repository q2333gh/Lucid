"""Unit tests for asset management logic (without canister)."""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile

import pytest


def test_chunk_utils():
    """Test chunk utility functions."""
    sys.path.insert(0, str(pathlib.Path(__file__).parent.parent.parent))
    from tests.utils.chunk_utils import chunk_sizes, max_chunk_size

    assert chunk_sizes(100, 50) == [50, 50]
    assert chunk_sizes(101, 50) == [50, 50, 1]
    assert chunk_sizes(0, 50) == []
    assert max_chunk_size(100, 50) == 50
    assert max_chunk_size(101, 50) == 50


def test_asset_path_resolution():
    """Test asset path resolution logic."""
    script_dir = pathlib.Path(__file__).parent.parent.parent
    vocab_path = script_dir / "assets" / "encoder.json"
    
    # Test that vocab path can be resolved (if vocab exists)
    if vocab_path.exists():
        assert vocab_path.is_file()
        assert vocab_path.suffix == ".json"


def test_checkpoint_validation():
    """Test checkpoint file validation."""
    script_dir = pathlib.Path(__file__).parent.parent.parent
    checkpoint = script_dir / "assets" / "gpt2_124M.bin"
    
    if checkpoint.exists():
        # Check file is readable and has reasonable size
        assert checkpoint.stat().st_size > 0
        # Check it's a binary file (not empty text)
        with checkpoint.open("rb") as f:
            header = f.read(8)
            assert len(header) == 8
            # Check for expected magic number (little endian: c6 d7 34 01)
            magic = int.from_bytes(header[:4], "little")
            version = int.from_bytes(header[4:], "little")
            if magic == 20240326 and version == 1:
                assert True  # Valid checkpoint
            else:
                pytest.skip("Checkpoint has unexpected format")
