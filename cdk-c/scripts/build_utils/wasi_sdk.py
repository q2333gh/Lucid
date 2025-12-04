#!/usr/bin/env python3
"""
WASI SDK discovery and configuration utilities.

Handles finding the WASI SDK installation and toolchain files.
"""

import os
from pathlib import Path
from typing import Optional, Tuple


def find_wasi_sdk_root() -> Optional[Path]:
    """
    Find WASI SDK root directory.

    Checks:
    1. WASI_SDK_ROOT environment variable
    2. Common installation locations

    Returns:
        Path to WASI SDK root, or None if not found
    """
    # 1. Check environment variable
    wasi_sdk_root = os.environ.get("WASI_SDK_ROOT")
    if wasi_sdk_root:
        path = Path(wasi_sdk_root)
        if path.exists():
            return path

    # 2. Check common locations
    common_paths = [
        Path("/opt/wasi-sdk"),
        Path("/usr/local/opt/wasi-sdk"),
        Path.home() / "opt/wasi-sdk",
        Path.home() / "wasi-sdk",
    ]

    for p in common_paths:
        if p.exists():
            return p

    return None


def find_toolchain_file(sdk_root: Path) -> Optional[Path]:
    """
    Find CMake toolchain file within WASI SDK.

    Args:
        sdk_root: Path to WASI SDK root

    Returns:
        Path to toolchain file, or None if not found
    """
    # Standard location
    toolchain = sdk_root / "share/cmake/wasi-sdk.cmake"
    if toolchain.exists():
        return toolchain

    # Versioned directory (wasi-sdk-xx.x)
    found = list(sdk_root.glob("share/cmake/wasi-sdk-*/wasi-sdk.cmake"))
    if found:
        return found[0]

    return None


def ensure_wasi_sdk() -> Tuple[Optional[Path], Optional[Path]]:
    """
    Ensure WASI SDK is available and find toolchain file.

    Returns:
        Tuple of (sdk_root_path, toolchain_file_path)
        Returns (None, None) if SDK not found
    """
    sdk_root = find_wasi_sdk_root()
    if not sdk_root:
        return None, None

    toolchain = find_toolchain_file(sdk_root)
    if not toolchain:
        print(
            f" Warning: Found WASI SDK at {sdk_root}, "
            "but could not find 'share/cmake/wasi-sdk.cmake'."
        )
        return None, None

    return sdk_root, toolchain
