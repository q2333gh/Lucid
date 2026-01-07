#!/usr/bin/env python3
"""
Builder functions for external dependencies.

Provides importable functions to build:
- libic_wasi_polyfill.a (IC WASI polyfill library)
- wasi2ic (WASI to IC converter tool)

These functions can be called directly from Python code without
spawning subprocess to run scripts.
"""

import os
import shutil
import sys
from pathlib import Path
from typing import Optional

# Handle imports for both package and direct loading
_script_dir = Path(__file__).parent.resolve()
if str(_script_dir) not in sys.path:
    sys.path.insert(0, str(_script_dir))

try:
    from config import IC_WASI_POLYFILL_COMMIT, WASI2IC_COMMIT
    from utils import run_quiet_cmd, command_exists, clone_repository_safe
except ImportError:
    from .config import IC_WASI_POLYFILL_COMMIT, WASI2IC_COMMIT
    from .utils import run_quiet_cmd, command_exists, clone_repository_safe


CARGO_BUILD_JOBS_LIMIT = 32


def _cargo_env_with_job_limit(max_jobs: int = CARGO_BUILD_JOBS_LIMIT) -> dict:
    """
    Return environment dict for cargo commands with a jobs limit applied.

    This keeps cargo compilations from consuming all CPU cores globally.
    """
    env = os.environ.copy()
    # Respect explicit user override, otherwise enforce limit.
    if "CARGO_BUILD_JOBS" not in env:
        env["CARGO_BUILD_JOBS"] = str(max_jobs)
    return env


# =============================================================================
# Toolchain Verification
# =============================================================================


def check_rust_toolchain(need_wasm_target: bool = False) -> bool:
    """
    Verify Rust toolchain is available.

    Args:
        need_wasm_target: If True, also check for wasm32-wasip1 target

    Returns:
        True if all required tools are available
    """
    import sh  # type: ignore

    required_tools = ["rustc", "cargo", "git"]
    if need_wasm_target:
        required_tools.append("rustup")

    missing = [t for t in required_tools if not command_exists(t)]
    if missing:
        print(f" Error: Missing tools: {', '.join(missing)}")
        return False

    # Check wasm target if needed
    if need_wasm_target:
        try:
            targets = sh.rustup("target", "list", "--installed").strip().split("\n")  # type: ignore
            if "wasm32-wasip1" not in targets:
                print(" Installing wasm32-wasip1 target...")
                print("   Installing wasm32-wasip1 target...")
                run_quiet_cmd(
                    "rustup",
                    "target",
                    "add",
                    "wasm32-wasip1",
                    raise_on_error=True,
                )
        except Exception as e:
            print(f" Error: Failed to setup wasm target: {e}")
            return False

    return True


# =============================================================================
# Polyfill Library Builder
# =============================================================================


def build_polyfill_library(
    output_dir: Path,
    version: Optional[str] = None,
    work_dir: Optional[Path] = None,
    features: Optional[str] = None,
    clean: bool = False,
) -> Optional[Path]:
    """
    Build libic_wasi_polyfill.a from source.

    Args:
        output_dir: Directory to place the built library
        version: Git tag/commit to build (default: IC_WASI_POLYFILL_COMMIT)
        work_dir: Working directory for clone (default: ~/.tmp_build_ic_wasi_polyfill)
        features: Cargo features to enable
        clean: If True, clean work directory before building

    Returns:
        Path to built library, or None if build failed
    """
    version = version or IC_WASI_POLYFILL_COMMIT
    work_dir = work_dir or Path.home() / ".tmp_build_ic_wasi_polyfill"

    print(" Building libic_wasi_polyfill.a...")
    print(f"   Version: {version}")

    # Check toolchain
    if not check_rust_toolchain(need_wasm_target=True):
        return None

    # Setup work directory
    work_dir.mkdir(parents=True, exist_ok=True)
    repo_name = "ic-wasi-polyfill"
    repo_path = work_dir / repo_name

    if clean and repo_path.exists():
        print(f"   Cleaning {repo_path}...")
        shutil.rmtree(repo_path)

    # Clone repository
    try:
        repo_url = "https://github.com/wasm-forge/ic-wasi-polyfill"
        repo_path = clone_repository_safe(repo_url, work_dir, version, repo_name)
    except Exception as e:
        print(f" Error: Clone failed: {e}")
        return None

    # Build with cargo
    cargo_args = ["build", "--release", "--target", "wasm32-wasip1"]
    if features:
        cargo_args.extend(["--features", features])

    try:
        print(f"   Compiling libic_wasi_polyfill (quiet, jobs<={CARGO_BUILD_JOBS_LIMIT})...")
        run_quiet_cmd(
            "cargo",
            *cargo_args,
            cwd=repo_path,
            env=_cargo_env_with_job_limit(),
            raise_on_error=True,
        )
    except Exception as e:
        print(f" Error: Build failed: {e}")
        return None

    # Find built library
    target_dir = repo_path / "target" / "wasm32-wasip1" / "release"
    lib_file = target_dir / "libic_wasi_polyfill.a"

    if not lib_file.exists():
        # Fallback search
        lib_files = list(target_dir.glob("*.a"))
        if lib_files:
            lib_file = lib_files[0]
        else:
            print(f" Error: Library not found in {target_dir}")
            return None

    # Copy to output directory
    output_dir.mkdir(parents=True, exist_ok=True)
    dest_file = output_dir / "libic_wasi_polyfill.a"
    shutil.copy2(lib_file, dest_file)

    size_kb = dest_file.stat().st_size / 1024
    print(f" Success: libic_wasi_polyfill.a ({size_kb:.1f} KB)")

    return dest_file


# =============================================================================
# wasi2ic Tool Builder
# =============================================================================


def build_wasi2ic_tool(
    output_dir: Path,
    version: Optional[str] = None,
    work_dir: Optional[Path] = None,
    clean: bool = False,
) -> Optional[Path]:
    """
    Build wasi2ic tool from source.

    Args:
        output_dir: Directory to place the built binary
        version: Git tag/commit to build (default: WASI2IC_COMMIT)
        work_dir: Working directory for clone (default: ~/.tmp_build_wasi2ic)
        clean: If True, clean work directory before building

    Returns:
        Path to built binary, or None if build failed
    """
    version = version or WASI2IC_COMMIT
    work_dir = work_dir or Path.home() / ".tmp_build_wasi2ic"

    print(" Building wasi2ic tool...")
    print(f"   Version: {version}")

    # Check toolchain
    if not check_rust_toolchain(need_wasm_target=False):
        return None

    # Setup work directory
    work_dir.mkdir(parents=True, exist_ok=True)
    repo_name = "wasi2ic"
    repo_path = work_dir / repo_name

    if clean and repo_path.exists():
        print(f"   Cleaning {repo_path}...")
        shutil.rmtree(repo_path)

    # Clone repository
    try:
        repo_url = "https://github.com/wasm-forge/wasi2ic"
        repo_path = clone_repository_safe(repo_url, work_dir, version, repo_name)
    except Exception as e:
        print(f" Error: Clone failed: {e}")
        return None

    # Build with cargo
    try:
        print(f"   Compiling wasi2ic (quiet, jobs<={CARGO_BUILD_JOBS_LIMIT})...")
        run_quiet_cmd(
            "cargo",
            "build",
            "--release",
            cwd=repo_path,
            env=_cargo_env_with_job_limit(),
            raise_on_error=True,
        )
    except Exception as e:
        print(f" Error: Build failed: {e}")
        return None

    # Find built binary
    target_dir = repo_path / "target" / "release"
    binary_file = target_dir / "wasi2ic"

    if not binary_file.exists():
        binary_file = target_dir / "wasi2ic.exe"  # Windows

    if not binary_file.exists():
        bin_files = list(target_dir.glob("wasi2ic*"))
        if bin_files:
            binary_file = bin_files[0]
        else:
            print(f" Error: Binary not found in {target_dir}")
            return None

    # Copy to output directory
    output_dir.mkdir(parents=True, exist_ok=True)
    dest_file = output_dir / binary_file.name
    shutil.copy2(binary_file, dest_file)

    # Make executable on Unix
    if os.name != "nt":
        os.chmod(dest_file, 0o755)

    size_mb = dest_file.stat().st_size / (1024 * 1024)
    print(f" Success: wasi2ic ({size_mb:.1f} MB)")

    return dest_file


# =============================================================================
# Convenience Functions
# =============================================================================


def ensure_polyfill_library(output_dir: Path) -> Optional[Path]:
    """
    Ensure libic_wasi_polyfill.a exists, build if needed.

    Args:
        output_dir: Directory where library should exist

    Returns:
        Path to library, or None if build failed
    """
    lib_path = output_dir / "libic_wasi_polyfill.a"
    if lib_path.exists():
        return lib_path
    return build_polyfill_library(output_dir)


def ensure_wasi2ic_tool(output_dir: Path) -> Optional[Path]:
    """
    Ensure wasi2ic tool exists, build if needed.

    Args:
        output_dir: Directory where tool should exist

    Returns:
        Path to tool, or None if build failed
    """
    tool_path = output_dir / "wasi2ic"
    if tool_path.exists():
        return tool_path

    # Check for .exe on Windows
    tool_path_exe = output_dir / "wasi2ic.exe"
    if tool_path_exe.exists():
        return tool_path_exe

    return build_wasi2ic_tool(output_dir)
