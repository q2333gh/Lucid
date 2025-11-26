#!/usr/bin/env python3
"""
Standalone script: Build libic_wasi_polyfill.a

Build libic_wasi_polyfill.a static library from
https://github.com/wasm-forge/ic-wasi-polyfill

Default version: b3ef005140e7eebf7d0b5471ccc3a6d4cbec4ee5

Usage:
    python3 build_libic_wasi_polyfill.py [--output-dir OUTPUT_DIR] [--clean]

Dependencies:
    - Python package: sh (pip install sh)
    - Rust/Cargo (must be installed on system)
    - wasm32-wasip1 target (script will install automatically)
    - git (for cloning repository)
"""

import argparse
import os
import shutil
import sys
from pathlib import Path
from typing import Optional

import sh


def run_cmd(
    cmd: str,
    cwd: Optional[Path] = None,
    check: bool = True,
    capture_output: bool = False,
):
    """Run shell command using sh library"""
    print(f"$ {cmd}")
    
    parts = cmd.split()
    if not parts:
        raise ValueError("Empty command")
    
    program = str(parts[0])
    args = [str(arg) for arg in parts[1:]]
    
    cmd_func = sh.Command(program)
    
    kwargs = {}
    if cwd:
        kwargs['_cwd'] = str(cwd)
    if capture_output:
        kwargs['_iter'] = False
    
    try:
        result = cmd_func(*args, **kwargs)
        if capture_output:
            stdout = result or ""
            if stdout:
                print(stdout)
            return type('Result', (), {
                'stdout': stdout,
                'stderr': '',
                'returncode': 0
            })()
    except sh.ErrorReturnCode as e:
        if capture_output and not check:
            stdout = e.stdout.decode('utf-8') if isinstance(e.stdout, bytes) else (e.stdout or "")
            stderr = e.stderr.decode('utf-8') if isinstance(e.stderr, bytes) else (e.stderr or "")
            return type('Result', (), {
                'stdout': stdout,
                'stderr': stderr,
                'returncode': e.exit_code
            })()
        if check:
            raise


def check_command_exists(cmd: str) -> bool:
    """Check if command exists"""
    try:
        if '/' in cmd:
            # Full path - check if file exists
            cmd_path = Path(cmd)
            return cmd_path.exists() and cmd_path.is_file()
        # Simple command name - let sh resolve it (raises CommandNotFound if missing)
        sh.Command(cmd)
        return True
    except sh.CommandNotFound:
        return False


def check_rust_installed() -> bool:
    """Check if Rust is installed"""
    try:
        result = sh.rustc("--version")
        version = result.strip()
        print(f"[green]Found Rust: {version.strip()}[/]")
        return True
    except (sh.ErrorReturnCode, AttributeError):
        return False


def check_cargo_installed() -> bool:
    """Check if Cargo is installed"""
    try:
        result = sh.cargo("--version")
        version = result.strip()
        print(f"[green]Found Cargo: {version.strip()}[/]")
        return True
    except (sh.ErrorReturnCode, AttributeError):
        return False


def check_wasm32_target_installed() -> bool:
    """Check if wasm32-wasip1 target is installed"""
    try:
        result = sh.rustup("target", "list", "--installed")
        output = result.strip()
        targets = output.strip().split("\n")
        if "wasm32-wasip1" in targets:
            print("[green]wasm32-wasip1 target is installed[/]")
            return True
        return False
    except (sh.ErrorReturnCode, AttributeError):
        return False


def install_wasm32_target() -> None:
    """Install wasm32-wasip1 target"""
    print("\n[bold]Installing wasm32-wasip1 target...[/]")
    run_cmd("rustup target add wasm32-wasip1", check=True)
    print("[green]wasm32-wasip1 target installed successfully[/]")


def clone_repository(repo_url: str, clone_dir: Path, version: str) -> Path:
    """Clone repository and switch to specified version"""
    repo_path = clone_dir / "ic-wasi-polyfill"

    if repo_path.exists():
        print(f"\n📂 Repository already exists at {repo_path}")
        print("   Updating to latest and switching to version...")
        run_cmd("git fetch origin", cwd=repo_path, check=True)
    else:
        print(f"\n📥 Cloning repository from {repo_url}...")
        run_cmd(f"git clone {repo_url} {repo_path}", cwd=clone_dir.parent, check=True)

    print(f"\n🔀 Switching to version: {version}")
    try:
        run_cmd(f"git checkout {version}", cwd=repo_path, check=True)
    except sh.ErrorReturnCode:
        # If tag doesn't exist, try fetching tags first
        print(f"   Tag not found locally, fetching tags...")
        run_cmd("git fetch --tags", cwd=repo_path, check=True)
        run_cmd(f"git checkout {version}", cwd=repo_path, check=True)

    # Verify current version
    result = run_cmd(
        "git describe --tags --exact-match HEAD 2>/dev/null || git rev-parse HEAD",
        cwd=repo_path,
        capture_output=True,
        check=True,
    )
    current_version = result.stdout.strip()
    print(f"[green]Current version: {current_version}[/]")

    return repo_path


def build_library(repo_path: Path, features: Optional[str] = None) -> Path:
    """Build libic_wasi_polyfill.a"""
    print(f"\n[bold]Building libic_wasi_polyfill.a...[/]")
    print(f"   Working directory: {repo_path}")

    cmd = "cargo build --release --target wasm32-wasip1"
    if features:
        cmd += f" --features {features}"

    run_cmd(cmd, cwd=repo_path, check=True)

    # Find generated library file
    target_dir = repo_path / "target" / "wasm32-wasip1" / "release"
    library_file = target_dir / "libic_wasi_polyfill.a"

    if not library_file.exists():
        # Try to find other possible library file names
        lib_files = list(target_dir.glob("*.a"))
        if lib_files:
            library_file = lib_files[0]
            print(f"[yellow]Found library file: {library_file.name}[/]")
        else:
            raise FileNotFoundError(
                f"Library file not found in {target_dir}. "
                f"Build may have failed or library name is different."
            )

    print(f"[green]Library built successfully: {library_file}[/]")
    return library_file


def main():
    parser = argparse.ArgumentParser(
        description="Build libic_wasi_polyfill.a from ic-wasi-polyfill repository",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Use default output directory (current directory)
  python3 build_libic_wasi_polyfill.py

  # Specify output directory
  python3 build_libic_wasi_polyfill.py --output-dir ./libs

  # Clean temporary files before building
  python3 build_libic_wasi_polyfill.py --clean

  # Build with transient feature
  python3 build_libic_wasi_polyfill.py --features transient
        """,
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default=".",
        help="Output directory (default: current directory)",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean temporary files before building",
    )
    parser.add_argument(
        "--version",
        type=str,
        default="b3ef005140e7eebf7d0b5471ccc3a6d4cbec4ee5",
        help="Version tag or commit hash to build ",
    )
    parser.add_argument(
        "--features",
        type=str,
        help="Cargo features, e.g.: transient",
    )
    parser.add_argument(
        "--work-dir",
        type=str,
        help="Work directory for cloning repository (default: temporary directory)",
    )

    args = parser.parse_args()

    print("=" * 70)
    print("Building libic_wasi_polyfill.a")
    print("=" * 70)

    # Check dependencies
    print("\n[bold]Checking dependencies...[/]")
    if not check_rust_installed():
        print("[red]Error: Rust is not installed[/]")
        print("   Please visit https://rustup.rs/ to install Rust")
        sys.exit(1)

    if not check_cargo_installed():
        print("[red]Error: Cargo is not installed[/]")
        print("   Please visit https://rustup.rs/ to install Rust (includes Cargo)")
        sys.exit(1)

    if not check_command_exists("git"):
        print("[red]Error: git is not installed[/]")
        print("   Please install git")
        sys.exit(1)

    # Check and install wasm32-wasip1 target
    if not check_wasm32_target_installed():
        install_wasm32_target()
    else:
        print("[green]wasm32-wasip1 target is already installed[/]")

    # Set work directory
    if args.work_dir:
        work_dir = Path(args.work_dir).resolve()
    else:
        work_dir = Path.home() / ".tmp_build_ic_wasi_polyfill"
    work_dir.mkdir(parents=True, exist_ok=True)

    # Clean option
    if args.clean and (work_dir / "ic-wasi-polyfill").exists():
        print(f"\n[bold]Cleaning work directory: {work_dir / 'ic-wasi-polyfill'}[/]")
        shutil.rmtree(work_dir / "ic-wasi-polyfill")

    # Clone repository and switch to specified version
    repo_url = "https://github.com/wasm-forge/ic-wasi-polyfill"
    repo_path = clone_repository(repo_url, work_dir, args.version)

    # Build library
    try:
        library_file = build_library(repo_path, args.features)

        # Copy to output directory
        output_dir = Path(args.output_dir).resolve()
        output_dir.mkdir(parents=True, exist_ok=True)
        output_file = output_dir / "libic_wasi_polyfill.a"

        print(f"\n📋 Copying library file to output directory...")
        shutil.copy2(library_file, output_file)
        print(f"[green]Library file copied to: {output_file}[/]")
        print(f"  File size: {output_file.stat().st_size / 1024:.2f} KB")

        print("\n" + "=" * 70)
        print("[green]Build complete![/]")
        print("=" * 70)
        print(f"[bold]Output file: {output_file}[/]")
        print(f"📂 Source file location: {library_file}")
        if not args.work_dir:
            print(f"\n💡 Tip: Temporary files are located at {work_dir}")
            print(f"   To clean up, run: rm -rf {work_dir}")

    except Exception as e:
        print(f"\n[red]Error: {e}[/]")
        if isinstance(e, sh.ErrorReturnCode):
            if hasattr(e, 'stdout') and e.stdout:
                stdout = e.stdout.decode('utf-8') if isinstance(e.stdout, bytes) else e.stdout
                print(f"Output: {stdout}")
            if hasattr(e, 'stderr') and e.stderr:
                stderr = e.stderr.decode('utf-8') if isinstance(e.stderr, bytes) else e.stderr
                print(f"Error: {stderr}")
        else:
            import traceback
            traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
