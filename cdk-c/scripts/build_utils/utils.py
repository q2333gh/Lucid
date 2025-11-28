#!/usr/bin/env python3
"""
Utility functions for IC C SDK build system
Command execution, file finding, and other helper functions
"""

import os
import sys
import shutil
import subprocess
from collections import deque
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
            kwargs['_cwd'] = str(cwd)
        
        # Run command
        cmd_func(*args, **kwargs)
        
        return True
    except sh.ErrorReturnCode as e:
        print(f"[red]Error: Command failed with exit code {e.exit_code}[/]")
        stderr = e.stderr.decode('utf-8') if isinstance(e.stderr, bytes) else (e.stderr or "")
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


def run_streaming_cmd(
    cmd_name: str,
    *args,
    cwd: Optional[Path] = None,
    title: str = "Processing...",
    max_lines: int = 4,
    raise_on_error: bool = True
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
                
            if raise_on_error:
                raise RuntimeError(f"Command failed with exit code {proc.returncode}")
            
        return proc.returncode
            
    except Exception as e:
        console.print(f"[bold red]Error executing command: {cmd_name}[/]")
        console.print(f"[red]{e}[/]")
        if raise_on_error:
            raise e
        return 1


def clone_repository_safe(repo_url: str, clone_dir: Path, version: str, repo_name: Optional[str] = None) -> Path:
    """
    Clone repository and switch to specified version (generic version).
    """
    name = repo_name or repo_url.split('/')[-1].replace('.git', '')
    repo_path = clone_dir / name

    if repo_path.exists():
        console.print(f"\n[bold cyan]Repository already exists at:[/] {repo_path}")
        console.print("   Updating to latest...")
        run_streaming_cmd("git", "fetch", "origin", cwd=repo_path, title=f"Fetching updates for {name}")
    else:
        console.print(f"\n[bold] Cloning repository...[/]")
        run_streaming_cmd("git", "clone", repo_url, str(repo_path), cwd=clone_dir.parent, title=f"Cloning {name}")

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


def find_polyfill_library(scripts_dir: Path) -> Path:
    """
    Find IC WASI Polyfill library
    
    Args:
        scripts_dir: Scripts directory path
    
    Returns:
        Path to polyfill library
    """
    # Preferred: Use scripts directory version (built from locked commit)
    scripts_polyfill = scripts_dir / "libic_wasi_polyfill.a"
    if scripts_polyfill.exists():
        return scripts_polyfill
    
    # Fallback: Try environment variable override
    env_polyfill = os.environ.get("IC_WASI_POLYFILL_PATH")
    if env_polyfill:
        env_path = Path(env_polyfill)
        if env_path.exists():
            return env_path
    
    # Default to scripts path (will be checked/used by ensure_polyfill_library)
    return scripts_polyfill


def find_wasi2ic_tool(scripts_dir: Path) -> Path:
    """
    Find wasi2ic tool. 
    
    Args:
        scripts_dir: Scripts directory path
    
    Returns:
        Path to wasi2ic tool
    """
    # Method 1: Environment variable override
    env_wasi2ic = os.environ.get("WASI2IC_PATH")
    if env_wasi2ic:
        env_path = Path(env_wasi2ic)
        if env_path.exists():
            console.print(f"[bold][cyan]Using wasi2ic tool from WASI2IC_PATH:[/] {env_path.resolve()}")
            return env_path

    # Method 2: Scripts directory (if built locally)
    scripts_wasi2ic = scripts_dir / "wasi2ic"
    if scripts_wasi2ic.exists():
        console.print(f"[bold][cyan]Using locally built wasi2ic tool:[/] {scripts_wasi2ic.resolve()}")
        return scripts_wasi2ic

    # Method 3: Common installation locations
    # Check if wasi2ic is in PATH
    wasi2ic_in_path = shutil.which("wasi2ic")
    if wasi2ic_in_path:
        console.print(f"[bold][cyan]Using wasi2ic tool from PATH:[/] {Path(wasi2ic_in_path).resolve()}")
        return Path(wasi2ic_in_path)

    # Default to scripts directory (will be checked/used when needed)
    console.print(f"[yellow]wasi2ic tool not found in environment, local scripts, or PATH. Defaulting to:[/] {scripts_wasi2ic.resolve()}")
    return scripts_wasi2ic

def ensure_polyfill_library(scripts_dir: Path, build_polyfill_script: Path) -> Optional[Path]:
    """
    Ensure libic_wasi_polyfill.a exists, build it if needed
    """
    # Method 1: Check scripts directory (preferred)
    scripts_polyfill = scripts_dir / "libic_wasi_polyfill.a"
    if scripts_polyfill.exists():
        console.print(f"\n[bold]Using IC WASI Polyfill library:[/]")
        console.print(f"   Path: {scripts_polyfill.resolve()}")
        
        return scripts_polyfill
    
    # Method 2: Check environment variable override
    env_polyfill = os.environ.get("IC_WASI_POLYFILL_PATH")
    if env_polyfill:
        env_path = Path(env_polyfill)
        if env_path.exists():
            console.print(f"\n[bold]Using IC WASI Polyfill library (from IC_WASI_POLYFILL_PATH):[/]")
            console.print(f"   Path: {env_path.resolve()}")
            
            return env_path
    
    # If neither exists, try to build it
    console.print(f"\n[yellow]libic_wasi_polyfill.a not found[/]")
    console.print("   Attempting to build it using build_libic_wasi_polyfill.py...")
    
    if not build_polyfill_script.exists():
        console.print(f"[red]Build script not found: {build_polyfill_script}[/]")
        console.print(f"   Please run build_libic_wasi_polyfill.py manually")
        console.print(f"   Expected commit: {IC_WASI_POLYFILL_COMMIT}")
        return None
    
    # Run the build script (should use locked commit)
    # Use run_streaming_cmd here too for better UX, though subprocess.run via run_command is fine too.
    # Since build_libic_wasi_polyfill.py is now interactive/rich-enabled, streaming it might be tricky 
    # if it tries to take over the console. But run_command just waits.
    # Let's stick to run_command for invoking another script to avoid rich-in-rich nesting issues 
    # unless we're careful.
    cmd = [
        sys.executable, 
        str(build_polyfill_script), 
        "--output-dir", str(scripts_dir),
        "--version", IC_WASI_POLYFILL_COMMIT
    ]
    
    # We use run_command here because the child script has its own Rich output.
    # If we used run_streaming_cmd, we'd capture its fancy output and print it as raw text lines
    # inside a panel, which might look weird (ansi codes double rendered).
    # Ideally, we just let it inherit stdout/stderr?
    # run_command uses sh.Command which captures or streams.
    # Let's just use subprocess.run to let it inherit stdio for best experience?
    # For now keeping run_command logic but maybe we should improve it.
    
    if not run_command(cmd, f"Building libic_wasi_polyfill.a"):
        console.print(f"[red]Failed to build libic_wasi_polyfill.a[/]")
        console.print(f"   Expected commit: {IC_WASI_POLYFILL_COMMIT}")
        return None
    
    if not scripts_polyfill.exists():
        console.print(f"[red]libic_wasi_polyfill.a still not found after build attempt[/]")
        return None
    
    console.print("[green]libic_wasi_polyfill.a is ready[/]")
    console.print(f"\n[bold]Using IC WASI Polyfill library (newly built):[/]")
    console.print(f"   Path: {scripts_polyfill.resolve()}")
    
    return scripts_polyfill


def ensure_wasi2ic_tool(scripts_dir: Path, build_wasi2ic_script: Path) -> Optional[Path]:
    """
    Ensure wasi2ic tool exists, build it if needed
    """
    # Re-check wasi2ic location
    wasi2ic = find_wasi2ic_tool(scripts_dir)
    
    # Method 1: Check if already exists
    if wasi2ic.exists():
        return wasi2ic
    
    # If not found, try to build it
    console.print(f"\n[yellow]wasi2ic tool not found[/]")
    console.print("   Attempting to build it using build_wasi2ic.py...")
    
    if not build_wasi2ic_script.exists():
        console.print(f"[red]Build script not found: {build_wasi2ic_script}[/]")
        console.print(f"   Please run build_wasi2ic.py manually")
        console.print(f"   Expected commit: {WASI2IC_COMMIT}")
        return None
    
    # Run the build script
    cmd = [
        sys.executable, 
        str(build_wasi2ic_script), 
        "--output-dir", str(scripts_dir),
        "--version", WASI2IC_COMMIT
    ]
    
    # Same logic as above: let the child script handle its own output
    if not run_command(cmd, f"Building wasi2ic tool"):
        console.print(f"[red]Failed to build wasi2ic tool[/]")
        console.print(f"   Expected commit: {WASI2IC_COMMIT}")
        return None
    
    wasi2ic = scripts_dir / "wasi2ic"
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


def needs_rebuild(lib_path: Path, src_dir: Path, source_files: list, target_platform: str, build_dir: Path) -> bool:
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


def optimize_wasm(wasm_file: Path, optimized_file: Path, optimization_level: str = "-Oz") -> bool:
    """
    Optimize WASM file using wasm-opt
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
    cmd = [wasm_opt_str, optimization_level, "--strip-debug", str(wasm_file), "-o", str(optimized_file)]
    
    if run_command(cmd, f"Optimizing {wasm_file.name} with wasm-opt"):
        if optimized_file.exists():
            original_size = wasm_file.stat().st_size
            optimized_size = optimized_file.stat().st_size
            reduction = ((original_size - optimized_size) / original_size) * 100
            console.print(f"[green]  Optimized: {wasm_file.name} ({original_size:,} bytes) -> {optimized_file.name} ({optimized_size:,} bytes, {reduction:.1f}% reduction)[/]")
            return True
        else:
            console.print(f"[yellow]  Optimization completed but output file not found: {optimized_file}[/]")
            return False
    else:
        console.print(f"[yellow]  wasm-opt optimization failed, but original WASM file is available: {wasm_file}[/]")
        return False
