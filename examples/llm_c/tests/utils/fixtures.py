"""Test data fixtures."""

from __future__ import annotations

import pathlib

SCRIPT_DIR = pathlib.Path(__file__).parent.parent.parent


def get_test_assets() -> dict[str, pathlib.Path]:
    """Get paths to test assets."""
    assets = {}
    
    assets_dir = SCRIPT_DIR / "assets"
    if assets_dir.exists():
        checkpoint = assets_dir / "gpt2_124M.bin"
        if checkpoint.exists():
            assets["checkpoint"] = checkpoint
        
        vocab_json = assets_dir / "encoder.json"
        vocab_bpe = assets_dir / "vocab.bpe"
        if vocab_json.exists():
            assets["vocab"] = vocab_json
        if vocab_bpe.exists():
            assets["merges"] = vocab_bpe
    
    return assets
