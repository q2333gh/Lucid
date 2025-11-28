#!/usr/bin/env python3
"""
Standalone script: Build libic_wasi_polyfill.a

Build libic_wasi_polyfill.a static library from
https://github.com/wasm-forge/ic-wasi-polyfill

Default version taken from build_utils/config.py (IC_WASI_POLYFILL_COMMIT)
"""

import os
import shutil
import subprocess
import sys
from collections import deque
from pathlib import Path
from typing import Optional

import sh
import typer
from rich.console import Console
from rich.live import Live
from rich.panel import Panel

# Allow importing sibling config when executed directly
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

try:
    from config import IC_WASI_POLYFILL_COMMIT
except ImportError:
    # Fallback or error if config is missing, though it should be there
    console = Console()
    console.print("[yellow]Warning: Could not import config.py[/]")
    IC_WASI_POLYFILL_COMMIT = "HEAD"

app = typer.Typer(help="Build libic_wasi_polyfill.a from source")
console = Console()

DEFAULT_POLYFILL_VERSION = IC_WASI_POLYFILL_COMMIT


def command_exists(cmd: str) -> bool:
    """Check whether a command/binary is available on the system."""
    if "/" in cmd or "\\" in cmd:
        cmd_path = Path(cmd)
        return cmd_path.exists() and os.access(cmd_path, os.X_OK)
    return shutil.which(cmd) is not None


def run_streaming_cmd(
    cmd_name: str,
    *args,
    cwd: Optional[Path] = None,
    title: str = "Processing...",
    max_lines: int = 4
):
    """
    Executes a shell command using rich's Live display to show the last few lines of output.
    """
    if not command_exists(cmd_name):
        console.print(f"[bold red]Command not found: {cmd_name}[/]")
        raise typer.Exit(code=1)

    cmd = [cmd_name, *[str(a) for a in args]]
    
    # Buffer to store recent lines
    buffer = deque(maxlen=max_lines)
    
    try:
        # Start the process
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            cwd=cwd
        )
        
        with Live(console=console, refresh_per_second=10) as live:
            # Initial display
            live.update(Panel("\n" * max_lines, title=title))
            
            # Read output line by line
            for line in proc.stdout:
                line_text = line.rstrip()
                buffer.append(line_text)
                
                # Update panel content
                content = "\n".join(buffer)
                live.update(Panel(content, title=f"{title} (last {max_lines} lines)"))
            
            proc.wait()
            
        if proc.returncode != 0:
            console.print(f"[bold red]Command failed with exit code {proc.returncode}[/]")
            # Print the captured buffer as context
            for line in buffer:
                console.print(f"[red]{line}[/]")
            raise typer.Exit(code=1)
            
    except Exception as e:
        console.print(f"[bold red]Error executing command: {cmd_name}[/]")
        console.print(f"[red]{e}[/]")
        raise typer.Exit(code=1)


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
            run_streaming_cmd("rustup", "target", "add", "wasm32-wasip1", title="Installing wasm32-wasip1 target")
            console.print("[green]Installed target: wasm32-wasip1[/]")
    except sh.ErrorReturnCode as err:
        console.print(f"[red]Failed to check/install rust targets: {err}[/]")
        raise typer.Exit(code=1)


def clone_repository(repo_url: str, clone_dir: Path, version: str) -> Path:
    """Clone repository and switch to specified version"""
    repo_path = clone_dir / "ic-wasi-polyfill"

    if repo_path.exists():
        console.print(f"\n[bold cyan]Repository already exists at:[/] {repo_path}")
        console.print("   Updating to latest...")
        run_streaming_cmd("git", "fetch", "origin", cwd=repo_path, title="Fetching updates")
    else:
        console.print(f"\n[bold] Cloning repository...[/]")
        run_streaming_cmd("git", "clone", repo_url, str(repo_path), cwd=clone_dir.parent, title="Cloning repository")

    console.print(f"\n[bold] Switching to version: {version}[/]")
    try:
        # Try direct checkout
        sh.git("checkout", version, _cwd=repo_path)
    except sh.ErrorReturnCode:
        # Fetch tags if needed
        console.print("   Tag not found locally, fetching tags...")
        run_streaming_cmd("git", "fetch", "--tags", cwd=repo_path, title="Fetching tags")
        sh.git("checkout", version, _cwd=repo_path)

    # Verify version
    try:
        current = sh.git("describe", "--tags", "--exact-match", "HEAD", _cwd=repo_path).strip()
    except sh.ErrorReturnCode:
        current = sh.git("rev-parse", "HEAD", _cwd=repo_path).strip()
    
    console.print(f"[green]Current version: {current}[/]")
    return repo_path


def build_library(repo_path: Path, features: Optional[str] = None) -> Path:
    """Build libic_wasi_polyfill.a"""
    console.print(f"\n[bold]Building libic_wasi_polyfill.a...[/]")
    
    cmd_args = ["build", "--release", "--target", "wasm32-wasip1"]
    if features:
        cmd_args.extend(["--features", features])

    run_streaming_cmd(
        "cargo", *cmd_args,
        cwd=repo_path, 
        title="Compiling libic_wasi_polyfill"
    )

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
        ".", 
        "--output-dir", "-o",
        help="Output directory for the library"
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
    repo_path = clone_repository(repo_url, working_path, version)
    library_file = build_library(repo_path, features)

    # Copy to output
    out_path = Path(output_dir).resolve()
    out_path.mkdir(parents=True, exist_ok=True)
    
    dest_file = out_path / "libic_wasi_polyfill.a"
    
    console.print(f"\n[bold]Copying to {dest_file}...[/]")
    shutil.copy2(library_file, dest_file)

    size_kb = dest_file.stat().st_size / 1024
    console.print(f"[green]Build complete! File size: {size_kb:.2f} KB[/]")
    console.print(f"[bold]Location: {dest_file}[/]")

    if not work_dir:
        console.print(f"\n[dim]Tip: Temporary files at {working_path}[/]")


if __name__ == "__main__":
    app()
