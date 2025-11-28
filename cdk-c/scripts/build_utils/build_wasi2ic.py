#!/usr/bin/env python3
"""
Standalone script: Build wasi2ic tool

Build wasi2ic binary from
https://github.com/wasm-forge/wasi2ic

Default version taken from build_utils/config.py (WASI2IC_COMMIT)
"""

import os
import shutil
import sys
from pathlib import Path
from typing import Optional

import sh
import typer
from rich.console import Console
from rich.panel import Panel

# Allow importing sibling config/utils when executed directly
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

try:
    from config import WASI2IC_COMMIT
except ImportError:
    # Fallback if config not found
    WASI2IC_COMMIT = "c0f5063e734f8365f1946baf2845d8322cc9bfec"

try:
    from utils import run_streaming_cmd, command_exists, clone_repository_safe
except ImportError:
    # Attempt to import from build_utils if running from parent
    try:
        from build_utils.utils import run_streaming_cmd, command_exists, clone_repository_safe
    except ImportError:
        print("Error: Could not import utils module.")
        sys.exit(1)

app = typer.Typer(help="Build wasi2ic tool from source")
console = Console()

PROJECT_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_OUTPUT_DIR = (PROJECT_ROOT / "build_lib").resolve()

DEFAULT_WASI2IC_VERSION = WASI2IC_COMMIT


def ensure_toolchain_ready():
    """Verify required tooling exists before running expensive work."""
    console.print("\n[bold]Checking dependencies...[/]")

    required_tools = ("rustc", "cargo", "git")
    missing_tools = [tool for tool in required_tools if not command_exists(tool)]

    if missing_tools:
        console.print(f"[red]Missing required tools: {', '.join(missing_tools)}[/]")
        console.print("   Please install them before rerunning the build.")
        raise typer.Exit(code=1)

    # After confirming availability, report versions for key tools.
    try:
        rust_version = sh.rustc("--version").strip()
        console.print(f"[green]Found Rust: {rust_version}[/]")
    except sh.ErrorReturnCode as err:
        console.print(f"[red]rustc reported an error: {err}[/]")
        raise typer.Exit(code=1)

    try:
        cargo_version = sh.cargo("--version").strip()
        console.print(f"[green]Found Cargo: {cargo_version}[/]")
    except sh.ErrorReturnCode as err:
        console.print(f"[red]cargo reported an error: {err}[/]")
        raise typer.Exit(code=1)


def build_binary(repo_path: Path) -> Path:
    """Build wasi2ic binary"""
    console.print(f"\n[bold]Building wasi2ic binary...[/]")
    
    try:
        run_streaming_cmd(
            "cargo", "build", "--release", 
            cwd=repo_path, 
            title="Compiling wasi2ic (release)"
        )
    except RuntimeError as e:
        console.print(f"[red]Build failed: {e}[/]")
        raise typer.Exit(code=1)

    target_dir = repo_path / "target" / "release"
    
    # Locate binary (handle Windows .exe)
    binary_file = target_dir / "wasi2ic"
    if not binary_file.exists():
        binary_file = target_dir / "wasi2ic.exe"
        
    if not binary_file.exists():
        # Fallback search
        bin_files = list(target_dir.glob("wasi2ic*"))
        if bin_files:
            binary_file = bin_files[0]
        else:
            console.print(f"[red]Binary not found in {target_dir}[/]")
            raise typer.Exit(code=1)

    console.print(f"[green]Binary built successfully: {binary_file}[/]")
    return binary_file


@app.command()
def main(
    output_dir: str = typer.Option(
        str(DEFAULT_OUTPUT_DIR), 
        "--output-dir", "-o",
        help="Output directory for the binary (default: cdk-c/build)"
    ),
    clean: bool = typer.Option(
        False, 
        "--clean", 
        help="Clean temporary files before building"
    ),
    version: str = typer.Option(
        DEFAULT_WASI2IC_VERSION, 
        "--version", "-v",
        help="Version tag or commit hash to build"
    ),
    work_dir: Optional[str] = typer.Option(
        None,
        "--work-dir", "-w",
        help="Custom work directory for cloning"
    )
):
    """
    Build the wasi2ic tool from source using Cargo.
    """
    ensure_toolchain_ready()
    console.print(Panel.fit("[bold]wasi2ic Build Tool[/]", border_style="green"))

    # Setup directories
    if work_dir:
        working_path = Path(work_dir).resolve()
        console.print(f"\n[bold]Using custom work directory: {working_path}[/]")
    else:
        working_path = Path.home() / ".tmp_build_wasi2ic"
        console.print(f"\n[bold]Using default work directory: {working_path}[/]")
    
    working_path.mkdir(parents=True, exist_ok=True)

    if clean and (working_path / "wasi2ic").exists():
        console.print(f"[yellow]Cleaning work directory...[/]")
        shutil.rmtree(working_path / "wasi2ic")

    # Process
    repo_url = "https://github.com/wasm-forge/wasi2ic"
    
    try:
        repo_path = clone_repository_safe(repo_url, working_path, version, repo_name="wasi2ic")
    except RuntimeError as e:
        console.print(f"[red]Cloning/checkout failed: {e}[/]")
        raise typer.Exit(code=1)
        
    binary_file = build_binary(repo_path)

    # Copy to output
    out_path = Path(output_dir).resolve()
    out_path.mkdir(parents=True, exist_ok=True)
    
    dest_file = out_path / binary_file.name
    
    console.print(f"\n[bold]Copying to {dest_file}...[/]")
    shutil.copy2(binary_file, dest_file)

    if os.name != 'nt':
        os.chmod(dest_file, 0o755)

    size_mb = dest_file.stat().st_size / (1024 * 1024)
    console.print(f"[green]Build complete! File size: {size_mb:.2f} MB[/]")
    console.print(f"[bold]Location: {dest_file}[/]")

    if not work_dir:
        console.print(f"\n[dim]Tip: Temporary files at {working_path}[/]")


if __name__ == "__main__":
    app()
