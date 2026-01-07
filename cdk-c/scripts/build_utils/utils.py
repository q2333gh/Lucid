#!/usr/bin/env python3
"""
Utility functions for IC C SDK build system
Command execution, file finding, and other helper functions
"""

import os
import sys
import shutil
import subprocess
from pathlib import Path
from typing import Optional, Tuple

import sh
from rich.console import Console
from rich.live import Live
from rich.panel import Panel

try:
    from .config import IC_WASI_POLYFILL_COMMIT, WASI2IC_COMMIT
except ImportError:
    from config import IC_WASI_POLYFILL_COMMIT, WASI2IC_COMMIT

console = Console(force_terminal=True, markup=True)
print = console.print  # route legacy prints through Rich


def command_exists(cmd: str) -> bool:
    """Check whether a command/binary is available on the system."""
    if "/" in cmd or "\\" in cmd:
        cmd_path = Path(cmd)
        return cmd_path.exists() and os.access(cmd_path, os.X_OK)
    return shutil.which(cmd) is not None


def run_command(cmd, description="", cwd=None, show_stderr=True):
    """
    Execute command and handle errors (non-streaming)

    Args:
        cmd: Command to execute (list of strings)
        description: Description of the command for output
        cwd: Working directory for command execution
        show_stderr: Whether to show stderr output

    Returns:
        bool: True if command succeeded, False otherwise
    """
    print(f"[bold]{description or ' '.join(cmd)}[/]")

    if not cmd:
        print("[red]Error: Empty command[/]")
        return False

    try:
        program = str(cmd[0])
        args = [str(arg) for arg in cmd[1:]] if len(cmd) > 1 else []

        cmd_func = sh.Command(program)

        kwargs = {}
        if cwd:
            kwargs["_cwd"] = str(cwd)

        # Run command
        cmd_func(*args, **kwargs)

        return True
    except sh.ErrorReturnCode as e:
        print(f"[red]Error: Command failed with exit code {e.exit_code}[/]")
        stderr = (
            e.stderr.decode("utf-8")
            if isinstance(e.stderr, bytes)
            else (e.stderr or "")
        )
        if stderr:
            print(f"Error details: {stderr}")
        return False
    except sh.CommandNotFound:
        print(f"[red]Error: Command not found: {cmd[0]}[/]")
        print("   Please ensure the corresponding compiler is installed")
        return False
    except Exception as e:
        print(f"[red]Error: {e}[/]")
        return False


def run_quiet_cmd(
    cmd_name: str,
    *args,
    cwd: Optional[Path] = None,
    env: Optional[dict] = None,
    raise_on_error: bool = True,
) -> int:
    """
    Execute a command without live/streaming output.

    Prints only a brief error summary on failure to keep logs quiet.
    """
    cmd = [cmd_name, *[str(a) for a in args]]
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
    except FileNotFoundError:
        console.print(f"[bold red]Command not found: {cmd_name}[/]")
        if raise_on_error:
            raise RuntimeError(f"Command not found: {cmd_name}")
        return 127
    except Exception as exc:
        console.print(f"[bold red]Error executing command: {cmd_name}[/]")
        console.print(f"[red]{exc}[/]")
        if raise_on_error:
            raise exc
        return 1

    if result.returncode != 0:
        console.print(f"[bold red]Command failed with exit code {result.returncode}[/]")
        if result.stdout:
            console.print(result.stdout.strip())
        if result.stderr:
            console.print(result.stderr.strip())
        if raise_on_error:
            raise RuntimeError(f"Command failed: {cmd_name}")

    return result.returncode


def run_streaming_cmd(
    cmd_name: str,
    *args,
    cwd: Optional[Path] = None,
    title: str = "Processing...",
    max_lines: int = 4,
    raise_on_error: bool = True,
) -> int:
    """
    Executes a shell command using rich's Live display to show the last few lines of output.

    Args:
        cmd_name: Command to run
        *args: Arguments for the command
        cwd: Working directory
        title: Title for the output panel
        max_lines: Max lines to show in the rolling buffer
        raise_on_error: Whether to raise an exception on non-zero exit code (default: True)

    Returns:
        int: Exit code

    Raises:
        RuntimeError: If raise_on_error is True and command fails
    """
    if not command_exists(cmd_name):
        console.print(f"[bold red]Command not found: {cmd_name}[/]")
        if raise_on_error:
            raise RuntimeError(f"Command not found: {cmd_name}")
        return 127

    cmd = [cmd_name, *[str(a) for a in args]]

    console.print(
        "[yellow]run_streaming_cmd is deprecated; use run_quiet_cmd for new code.[/]"
    )
    try:
        completed = subprocess.run(
            cmd,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
        if completed.returncode != 0:
            console.print(
                f"[bold red]Command failed with exit code {completed.returncode}[/]"
            )
            if completed.stdout:
                console.print(completed.stdout.strip())
            if raise_on_error:
                raise RuntimeError(
                    f"Command failed with exit code {completed.returncode}"
                )
        return completed.returncode
    except Exception as exc:
        console.print(f"[bold red]Error executing command: {cmd_name}[/]")
        console.print(f"[red]{exc}[/]")
        if raise_on_error:
            raise exc
        return 1


def clone_repository_safe(
    repo_url: str, clone_dir: Path, version: str, repo_name: Optional[str] = None
) -> Path:
    """
    Clone repository and switch to specified version (generic version).
    """
    name = repo_name or repo_url.split("/")[-1].replace(".git", "")
    repo_path = clone_dir / name

    if repo_path.exists():
        console.print(f"\n[bold cyan]Repository already exists at:[/] {repo_path}")
        console.print("   Updating to latest...")
        run_quiet_cmd("git", "fetch", "origin", cwd=repo_path, raise_on_error=True)
    else:
        console.print(f"\n[bold] Cloning repository...[/]")
        run_quiet_cmd(
            "git",
            "clone",
            repo_url,
            str(repo_path),
            cwd=clone_dir.parent,
            raise_on_error=True,
        )

    console.print(f"\n[bold] Switching to version: {version}[/]")
    try:
        # Try direct checkout
        sh.git("checkout", version, _cwd=repo_path)
    except sh.ErrorReturnCode:
        # Fetch tags if needed
        console.print("   Tag not found locally, fetching tags...")
        run_quiet_cmd("git", "fetch", "--tags", cwd=repo_path, raise_on_error=True)
        sh.git("checkout", version, _cwd=repo_path)

    # Verify version
    try:
        current = sh.git(
            "describe", "--tags", "--exact-match", "HEAD", _cwd=repo_path
        ).strip()
    except sh.ErrorReturnCode:
        current = sh.git("rev-parse", "HEAD", _cwd=repo_path).strip()

    console.print(f"[green]Current version: {current}[/]")
    return repo_path


def find_polyfill_library(artifact_dir: Path) -> Path:
    """
    Find IC WASI Polyfill library in the specific artifact directory.

    Args:
        artifact_dir: Directory where the polyfill library should reside (build_lib).

    Returns:
        Path to polyfill library
    """
    return artifact_dir / "libic_wasi_polyfill.a"


def find_wasi2ic_tool(artifact_dir: Path) -> Path:
    """
    Find wasi2ic tool in the specific artifact directory.

    Args:
        artifact_dir: Directory where the wasi2ic tool should reside (build_lib).

    Returns:
        Path to wasi2ic tool
    """
    # Check for binary with or without extension
    wasi2ic = artifact_dir / "wasi2ic"
    if wasi2ic.exists():
        return wasi2ic

    wasi2ic_exe = artifact_dir / "wasi2ic.exe"
    if wasi2ic_exe.exists():
        return wasi2ic_exe

    return wasi2ic


def ensure_polyfill_library(
    artifact_dir: Path, build_polyfill_script: Path
) -> Optional[Path]:
    """
    Ensure libic_wasi_polyfill.a exists in artifact_dir, build it if needed.

    Args:
        artifact_dir: Directory where artifacts should be stored
        build_polyfill_script: Path to the build script
    """
    polyfill_lib = find_polyfill_library(artifact_dir)

    if polyfill_lib.exists():
        return polyfill_lib

    # If not exists, build it
    console.print(f"\n[yellow]libic_wasi_polyfill.a not found in {artifact_dir}[/]")
    console.print("   Attempting to build it...")

    if not build_polyfill_script.exists():
        console.print(f"[red]Build script not found: {build_polyfill_script}[/]")
        return None

    # Ensure artifact directory exists
    artifact_dir.mkdir(parents=True, exist_ok=True)

    # Build directly to artifact_dir
    cmd = [
        sys.executable,
        str(build_polyfill_script),
        "--output-dir",
        str(artifact_dir),
        "--version",
        IC_WASI_POLYFILL_COMMIT,
    ]

    if not run_command(cmd, f"Building libic_wasi_polyfill.a"):
        console.print(f"[red]Failed to build libic_wasi_polyfill.a[/]")
        return None

    if not polyfill_lib.exists():
        console.print(
            f"[red]libic_wasi_polyfill.a still not found after build attempt[/]"
        )
        return None

    console.print("[green]libic_wasi_polyfill.a is ready[/]")

    return polyfill_lib


def ensure_wasi2ic_tool(
    artifact_dir: Path, build_wasi2ic_script: Path
) -> Optional[Path]:
    """
    Ensure wasi2ic tool exists in artifact_dir, build it if needed.

    Args:
        artifact_dir: Directory where artifacts should be stored
        build_wasi2ic_script: Path to the build script
    """
    wasi2ic = find_wasi2ic_tool(artifact_dir)

    if wasi2ic.exists():
        console.print(f"\n[bold]Using wasi2ic tool:[/]")
        console.print(f"   Path: {wasi2ic.resolve()}")
        return wasi2ic

    # If not exists, build it
    console.print(f"\n[yellow]wasi2ic tool not found in {artifact_dir}[/]")
    console.print("   Attempting to build it...")

    if not build_wasi2ic_script.exists():
        console.print(f"[red]Build script not found: {build_wasi2ic_script}[/]")
        return None

    # Ensure artifact directory exists
    artifact_dir.mkdir(parents=True, exist_ok=True)

    # Build directly to artifact_dir
    cmd = [
        sys.executable,
        str(build_wasi2ic_script),
        "--output-dir",
        str(artifact_dir),
        "--version",
        WASI2IC_COMMIT,
    ]

    if not run_command(cmd, f"Building wasi2ic tool"):
        console.print(f"[red]Failed to build wasi2ic tool[/]")
        return None

    # Refresh path in case it was built with/without exe extension
    wasi2ic = find_wasi2ic_tool(artifact_dir)

    if not wasi2ic.exists():
        console.print(f"[red]wasi2ic tool still not found after build attempt[/]")
        return None

    console.print("[green]wasi2ic tool is ready[/]")
    console.print(f"\n[bold]Using wasi2ic tool (newly built):[/]")
    console.print(f"   Path: {wasi2ic.resolve()}")

    return wasi2ic


def check_source_files_exist(src_dir: Path, source_files: list) -> Tuple[bool, list]:
    """
    Check if all source files exist
    """
    missing_files = []
    for src in source_files:
        if not (src_dir / src).exists():
            missing_files.append(src)

    return (len(missing_files) == 0, missing_files)


def needs_rebuild(
    lib_path: Path,
    src_dir: Path,
    source_files: list,
    target_platform: str,
    build_dir: Path,
) -> bool:
    """
    Check if library needs to be rebuilt
    """
    if not lib_path.exists():
        return True

    # For WASI platform, verify object files exist
    if target_platform == "wasi":
        obj_file = build_dir / source_files[0].replace(".c", ".o")
        if not obj_file.exists():
            return True

    # Check if any source file is newer than the library
    lib_mtime = lib_path.stat().st_mtime
    for src in source_files:
        src_path = src_dir / src
        if src_path.exists() and src_path.stat().st_mtime > lib_mtime:
            return True

    return False


def find_wasm_opt() -> Optional[Path]:
    """
    Find wasm-opt tool
    """
    wasm_opt_in_path = shutil.which("wasm-opt")
    if wasm_opt_in_path:
        return Path(wasm_opt_in_path)
    return None


def optimize_wasm(
    wasm_file: Path, optimized_file: Path, optimization_level: str = "-O4"
) -> bool:
    """
    Optimize WASM file using wasm-opt
    Oz for binary size
    O4 for computation  heavy tasks
    """
    wasm_opt = find_wasm_opt()
    if not wasm_opt:
        console.print("[yellow]  wasm-opt not found in PATH, skipping optimization[/]")
        console.print("     Install binaryen package: sudo apt install binaryen")
        console.print("     Or: brew install binaryen (on macOS)")
        return False

    if not wasm_file.exists():
        console.print(f"[red]  Input WASM file not found: {wasm_file}[/]")
        return False

    wasm_opt_str = str(wasm_opt) if isinstance(wasm_opt, Path) else wasm_opt
    cmd = [
        wasm_opt_str,
        optimization_level,
        "--strip-debug",
        str(wasm_file),
        "-o",
        str(optimized_file),
    ]

    if run_command(cmd):
        if optimized_file.exists():
            original_size = wasm_file.stat().st_size
            optimized_size = optimized_file.stat().st_size
            reduction = ((original_size - optimized_size) / original_size) * 100
            console.print(
                f"[green]  Optimized: {wasm_file.name} ({original_size:,} bytes) -> {optimized_file.name} ({optimized_size:,} bytes, {reduction:.1f}% reduction)[/]"
            )
            return True
        else:
            console.print(
                f"[yellow]  Optimization completed but output file not found: {optimized_file}[/]"
            )
            return False
    else:
        console.print(
            f"[yellow]  wasm-opt optimization failed, but original WASM file is available: {wasm_file}[/]"
        )
        return False
