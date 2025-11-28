#!/usr/bin/env python3
"""
Standalone script: Build libic_wasi_polyfill.a

Build libic_wasi_polyfill.a static library from
https://github.com/wasm-forge/ic-wasi-polyfill

Default version taken from build_utils/config.py (IC_WASI_POLYFILL_COMMIT)
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

# Allow importing sibling config when executed directly
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

try:
    from config import IC_WASI_POLYFILL_COMMIT
except ImportError:
    # Fallback or error if config is missing
    console = Console()
    console.print("[yellow]Warning: Could not import config.py[/]")
    IC_WASI_POLYFILL_COMMIT = "HEAD"

try:
    from utils import run_streaming_cmd, command_exists, clone_repository_safe
except ImportError:
    try:
        from build_utils.utils import run_streaming_cmd, command_exists, clone_repository_safe
    except ImportError:
        print("Error: Could not import utils module.")
        sys.exit(1)

app = typer.Typer(help="Build libic_wasi_polyfill.a from source")
console = Console()

PROJECT_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_OUTPUT_DIR = (PROJECT_ROOT / "build_lib").resolve()

DEFAULT_POLYFILL_VERSION = IC_WASI_POLYFILL_COMMIT


def ensure_toolchain_ready():
    """Verify required tooling exists before running expensive work."""
    console.print("\n[bold]Checking dependencies...[/]")

    # 1. Check basic tools
    required_tools = ("rustc", "cargo", "git", "rustup")
    missing_tools = [tool for tool in required_tools if not command_exists(tool)]

    if missing_tools:
        console.print(f"[red]Missing required tools: {', '.join(missing_tools)}[/]")
        console.print("   Please install them before rerunning the build.")
        raise typer.Exit(code=1)

    # 2. Report versions
    try:
        rust_version = sh.rustc("--version").strip()
        console.print(f"[green]Found Rust: {rust_version}[/]")
        cargo_version = sh.cargo("--version").strip()
        console.print(f"[green]Found Cargo: {cargo_version}[/]")
    except sh.ErrorReturnCode as err:
        console.print(f"[red]Tool check failed: {err}[/]")
        raise typer.Exit(code=1)

    # 3. Check/Install wasm32-wasip1 target
    try:
        targets = sh.rustup("target", "list", "--installed").strip().split("\n")
        if "wasm32-wasip1" in targets:
            console.print("[green]Found target: wasm32-wasip1[/]")
        else:
            console.print("[yellow]Target wasm32-wasip1 not found. Installing...[/]")
            try:
                run_streaming_cmd("rustup", "target", "add", "wasm32-wasip1", title="Installing wasm32-wasip1 target")
                console.print("[green]Installed target: wasm32-wasip1[/]")
            except RuntimeError as e:
                console.print(f"[red]Failed to install target: {e}[/]")
                raise typer.Exit(code=1)
    except sh.ErrorReturnCode as err:
        console.print(f"[red]Failed to check/install rust targets: {err}[/]")
        raise typer.Exit(code=1)


def build_library(repo_path: Path, features: Optional[str] = None) -> Path:
    """Build libic_wasi_polyfill.a"""
    console.print(f"\n[bold]Building libic_wasi_polyfill.a...[/]")
    
    cmd_args = ["build", "--release", "--target", "wasm32-wasip1"]
    if features:
        cmd_args.extend(["--features", features])

    try:
        run_streaming_cmd(
            "cargo", *cmd_args,
            cwd=repo_path, 
            title="Compiling libic_wasi_polyfill"
        )
    except RuntimeError as e:
        console.print(f"[red]Build failed: {e}[/]")
        raise typer.Exit(code=1)

    # Find generated library file
    target_dir = repo_path / "target" / "wasm32-wasip1" / "release"
    library_file = target_dir / "libic_wasi_polyfill.a"

    if not library_file.exists():
        # Fallback search
        lib_files = list(target_dir.glob("*.a"))
        if lib_files:
            library_file = lib_files[0]
            console.print(f"[yellow]Found library file: {library_file.name}[/]")
        else:
            console.print(f"[red]Library file not found in {target_dir}[/]")
            raise typer.Exit(code=1)

    console.print(f"[green]Library built successfully: {library_file}[/]")
    return library_file


@app.command()
def main(
    output_dir: str = typer.Option(
        str(DEFAULT_OUTPUT_DIR), 
        "--output-dir", "-o",
        help="Output directory for the library (default: cdk-c/build)"
    ),
    clean: bool = typer.Option(
        False, 
        "--clean", 
        help="Clean temporary files before building"
    ),
    version: str = typer.Option(
        DEFAULT_POLYFILL_VERSION, 
        "--version", "-v",
        help="Version tag or commit hash to build"
    ),
    features: Optional[str] = typer.Option(
        None,
        "--features", "-f",
        help="Cargo features to enable (e.g., transient)"
    ),
    work_dir: Optional[str] = typer.Option(
        None,
        "--work-dir", "-w",
        help="Custom work directory for cloning"
    )
):
    """
    Build libic_wasi_polyfill.a from source.
    """
    ensure_toolchain_ready()
    console.print(Panel.fit("[bold]IC WASI Polyfill Builder[/]", border_style="green"))

    # Setup directories
    if work_dir:
        working_path = Path(work_dir).resolve()
        console.print(f"\n[bold]Using custom work directory: {working_path}[/]")
    else:
        working_path = Path.home() / ".tmp_build_ic_wasi_polyfill"
        console.print(f"\n[bold]Using default work directory: {working_path}[/]")
    
    working_path.mkdir(parents=True, exist_ok=True)

    if clean and (working_path / "ic-wasi-polyfill").exists():
        console.print(f"[yellow]Cleaning work directory...[/]")
        shutil.rmtree(working_path / "ic-wasi-polyfill")

    # Process
    repo_url = "https://github.com/wasm-forge/ic-wasi-polyfill"
    
    try:
        repo_path = clone_repository_safe(repo_url, working_path, version, repo_name="ic-wasi-polyfill")
    except RuntimeError as e:
        console.print(f"[red]Cloning/checkout failed: {e}[/]")
        raise typer.Exit(code=1)
        
    library_file = build_library(repo_path, features)

    # Copy to output
    out_path = Path(output_dir).resolve()
    out_path.mkdir(parents=True, exist_ok=True)
    
    dest_file = out_path / "libic_wasi_polyfill.a"
    
    console.print(f"\n[bold]Copying to {dest_file}...[/]")
    shutil.copy2(library_file, dest_file)

    size_kb = dest_file.stat().st_size / 1024
    console.print(f"[green]Build complete! File size: {size_kb:.2f} KB[/]")
    console.print(f"[bold]Result target location: {dest_file}[/]")

    if not work_dir:
        console.print(f"\n[dim]Tip: Temporary files at {working_path}[/]")


if __name__ == "__main__":
    app()
