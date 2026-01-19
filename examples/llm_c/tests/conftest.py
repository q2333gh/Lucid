"""Shared pytest fixtures for llm_c tests."""

from __future__ import annotations

import pathlib
import sys
from typing import Generator

import pytest

# Add parent directory to path for imports
SCRIPT_DIR = pathlib.Path(__file__).resolve().parent.parent
REPO_ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(SCRIPT_DIR))
sys.path.insert(0, str(REPO_ROOT))


@pytest.fixture
def example_dir() -> pathlib.Path:
    """Return the examples/llm_c directory."""
    return SCRIPT_DIR


@pytest.fixture
def repo_root() -> pathlib.Path:
    """Return the repository root directory."""
    return REPO_ROOT


@pytest.fixture
def vocab_dir(example_dir: pathlib.Path) -> pathlib.Path:
    """Return the vocab directory path."""
    vocab_path = example_dir / "assets"
    if not vocab_path.exists():
        pytest.skip("assets directory not found")
    return vocab_path


@pytest.fixture
def checkpoint_path(example_dir: pathlib.Path) -> pathlib.Path | None:
    """Return the checkpoint file path if it exists."""
    checkpoint = example_dir / "assets" / "gpt2_124M.bin"
    if checkpoint.exists():
        return checkpoint
    return None
