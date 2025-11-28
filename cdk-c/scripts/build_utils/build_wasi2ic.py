#!/usr/bin/env python3
"""
Standalone script: Build wasi2ic tool

Build wasi2ic binary from
https://github.com/wasm-forge/wasi2ic

Default version taken from build_utils/config.py (WASI2IC_COMMIT)
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
    from config import WASI2IC_COMMIT
except ImportError:
    # Fallback if config not found (e.g. moved)
    WASI2IC_COMMIT = "c0f5063e734f8365f1946baf2845d8322cc9bfec"

app = typer.Typer(help="Build wasi2ic tool from source")
console = Console()

DEFAULT_WASI2IC_VERSION = WASI2IC_COMMIT


def command_exists(cmd: str) -> bool:
    """Check whether a command/binary is available on the system."""
    if "/" in cmd or "\\" in cmd:
        cmd_path = Path(cmd)
        return cmd_path.exists() and os.access(cmd_path, os.X_OK)
    return shutil.which(cmd) is not None


def resolve_command(cmd_name: str) -> sh.Command:
    """Resolve command name to an sh.Command, exiting early if missing."""
    if not command_exists(cmd_name):
        console.print(f"[bold red]Command not found: {cmd_name}[/]")
        raise typer.Exit(code=1)
    return sh.Command(cmd_name)


def run_streaming_cmd(
    cmd_name: str,
    *args,
    cwd: Optional[Path] = None,
    title: str = "Processing...",
    max_lines: int = 4
):
    """
    Executes a shell command using rich's Live display to show the last few lines of output.
    Inspired by run_and_tail from cdk-c/a.py.
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

    # git availability already confirmed; no version needed for success path.


def clone_repository(repo_url: str, clone_dir: Path, version: str) -> Path:
    """Clone repository and switch to specified version"""
    repo_path = clone_dir / "wasi2ic"

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


def build_binary(repo_path: Path) -> Path:
    """Build wasi2ic binary"""
    console.print(f"\n[bold]Building wasi2ic binary...[/]")
    
    run_streaming_cmd(
        "cargo", "build", "--release", 
        cwd=repo_path, 
        title="Compiling wasi2ic (release)"
    )

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
        ".", 
        "--output-dir", "-o",
        help="Output directory for the binary"
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
    repo_path = clone_repository(repo_url, working_path, version)
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
