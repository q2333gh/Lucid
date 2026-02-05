#!/usr/bin/env python3
"""
Unified core test entrypoint for the Lucid IC C SDK.

Goals:
- Provide a single, easy-to-remember command to validate the core SDK.
- Reuse the existing build pipeline in build.py instead of duplicating logic.
- Run IC WASM build to ensure SDK compiles correctly for IC platform.
- Run native C/CMake tests (cdk-c tests, test/c_candid tests).
- Run Python-level core tests that use PocketIC (inter-canister-call, basic_test).

Usage (from repo root):

    python test/run_core_tests.py

This script follows the testing framework plan:
1. Build IC WASM (validates SDK compiles for IC platform)
2. Build and run native C tests (via `python build.py --test`)
3. Run Python PocketIC integration tests

This script intentionally keeps the logic simple and explicit so it is easy to
maintain and extend later (for example, to add more pytest targets).
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def find_repo_root() -> Path:
    """
    Locate the repository root by searching upwards for build.py.

    Falls back to the current directory of this file if not found.
    """
    start = Path(__file__).resolve()
    for parent in [start] + list(start.parents):
        if (parent / "build.py").exists():
            return parent
    return start.parent


def run(cmd: list[str], cwd: Path, description: str = "") -> int:
    """
    Run a command, streaming output, and return its exit code.
    """
    if description:
        sys.stderr.write(f"[core-tests] {description}\n")
    sys.stderr.write(f"[core-tests] Running: {' '.join(cmd)} (cwd={cwd})\n")
    sys.stderr.flush()
    completed = subprocess.run(cmd, cwd=str(cwd))
    return completed.returncode


def main() -> int:
    repo_root = find_repo_root()

    # Step 1: Build IC WASM (validates SDK compiles for IC platform)
    # This follows the plan requirement: "Run python build.py --icwasm"
    icwasm_cmd = [sys.executable, "build.py", "--icwasm"]
    code = run(icwasm_cmd, cwd=repo_root, description="Step 1: Building IC WASM...")
    if code != 0:
        sys.stderr.write("[core-tests] IC WASM build failed.\n")
        return code

    # Step 2: Native build + C/CTest-based tests
    # This runs cdk-c/test/* and test/c_candid/* tests via CMake/ctest
    build_test_cmd = [sys.executable, "build.py", "--test"]
    code = run(
        build_test_cmd,
        cwd=repo_root,
        description="Step 2: Building and running native C tests...",
    )
    if code != 0:
        sys.stderr.write("[core-tests] Native build or C tests failed.\n")
        return code

    # Step 3: Python-level core tests that live under test/
    # These are focused on core SDK behavior, not examples.
    # They use PocketIC for integration testing.
    pytest_targets = [
        "test/inter-canister-call",
        "test/basic_test",
    ]

    pytest_cmd = [sys.executable, "-m", "pytest", "-s", *pytest_targets]
    code = run(
        pytest_cmd,
        cwd=repo_root,
        description="Step 3: Running Python PocketIC integration tests...",
    )
    if code != 0:
        sys.stderr.write("[core-tests] Python core tests failed.\n")
        return code

    sys.stderr.write("[core-tests] ✓ All core tests passed.\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

