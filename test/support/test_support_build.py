#!/usr/bin/env python3
"""
Build- and path-related helpers for example canisters.

This module is intentionally generic so it can be reused for multiple
examples. It:
  - locates the project root (where build.py lives)
  - runs `python build.py --icwasm --examples <example>`
  - resolves the resulting `<example>_ic.wasm` and `<example>.did` paths
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from typing import Tuple


def find_repo_root(start: Path | None = None) -> Path:
    """
    Locate the repository root by searching upwards for build.py.
    Falls back to the current directory if not found.
    """
    if start is None:
        start = Path(__file__).resolve()

    for parent in [start] + list(start.parents):
        if (parent / "build.py").exists():
            return parent

    return start


def build_example_ic_wasm(example_name: str) -> None:
    """
    Run `python build.py --icwasm --examples <example_name>` from the repo root.

    Raises subprocess.CalledProcessError if the build fails.
    """
    repo_root = find_repo_root()
    cmd = [
        sys.executable,
        "build.py",
        "--icwasm",
        "--examples",
        example_name,
    ]
    print(f"[build] Running: {' '.join(cmd)} (cwd={repo_root})")
    # Capture and display build output in real-time
    process = subprocess.Popen(
        cmd,
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,  # Line buffered
    )
    
    # Print output line by line
    for line in process.stdout:
        print(f"[build] {line.rstrip()}")
    
    process.wait()
    if process.returncode != 0:
        raise subprocess.CalledProcessError(process.returncode, cmd)


def get_wasm_and_did_paths(example_name: str) -> Tuple[Path, Path]:
    """
    Resolve the paths to:
      - build-wasi/bin/<example_name>_ic.wasm
      - examples/<example_name>/<example_name>.did
    relative to the repo root.
    """
    repo_root = find_repo_root()

    bin_dir = repo_root / "build-wasi" / "bin"
    if not bin_dir.exists():
        raise FileNotFoundError(
            f"Bin directory not found: {bin_dir}. Did you run build_example_ic_wasm?"
        )

    wasm_name = f"{example_name}_ic.wasm"
    wasm_path = bin_dir / wasm_name
    if not wasm_path.exists():
        raise FileNotFoundError(
            f"WASM not found: {wasm_path}. "
            f"Make sure example '{example_name}' is built and post-processed."
        )

    did_path = repo_root / "examples" / example_name / f"{example_name}.did"
    if not did_path.exists():
        raise FileNotFoundError(
            f"DID file not found: {did_path}. "
            f"Expected '{example_name}.did' next to the example sources."
        )

    return wasm_path, did_path
