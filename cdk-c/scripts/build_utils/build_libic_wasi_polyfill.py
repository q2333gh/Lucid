#!/usr/bin/env python3
"""
Standalone script: Build libic_wasi_polyfill.a

Build libic_wasi_polyfill.a static library from
https://github.com/wasm-forge/ic-wasi-polyfill

Default version: b3ef005140e7eebf7d0b5471ccc3a6d4cbec4ee5

Usage:
    python3 build_libic_wasi_polyfill.py [--output-dir OUTPUT_DIR] [--clean]

Dependencies:
    - Rust/Cargo (must be installed on system)
    - wasm32-wasip1 target (script will install automatically)
    - git (for cloning repository)
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional


def run_cmd(
    cmd: str,
    cwd: Optional[Path] = None,
    check: bool = True,
    capture_output: bool = False,
) -> subprocess.CompletedProcess:
    """Run shell command"""
    print(f"$ {cmd}")
    result = subprocess.run(
        cmd,
        shell=True,
        cwd=cwd,
        check=check,
        capture_output=capture_output,
        text=True,
    )
    if capture_output and result.stdout:
        print(result.stdout)
    return result


def check_command_exists(cmd: str) -> bool:
    """Check if command exists"""
    try:
        subprocess.run(
            ["which", cmd],
            capture_output=True,
            check=True,
        )
        return True
    except subprocess.CalledProcessError:
        return False


def check_rust_installed() -> bool:
    """Check if Rust is installed"""
    try:
        result = subprocess.run(
            ["rustc", "--version"],
            capture_output=True,
            check=True,
        )
        print(f"✓ Found Rust: {result.stdout.decode().strip()}")
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def check_cargo_installed() -> bool:
    """Check if Cargo is installed"""
    try:
        result = subprocess.run(
            ["cargo", "--version"],
            capture_output=True,
            check=True,
        )
        print(f"✓ Found Cargo: {result.stdout.decode().strip()}")
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def check_wasm32_target_installed() -> bool:
    """Check if wasm32-wasip1 target is installed"""
    try:
        result = subprocess.run(
            ["rustup", "target", "list", "--installed"],
            capture_output=True,
            check=True,
            text=True,
        )
        targets = result.stdout.strip().split("\n")
        if "wasm32-wasip1" in targets:
            print("✓ wasm32-wasip1 target is installed")
            return True
        return False
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def install_wasm32_target() -> None:
    """Install wasm32-wasip1 target"""
    print("\n📦 Installing wasm32-wasip1 target...")
    run_cmd("rustup target add wasm32-wasip1", check=True)
    print("✓ wasm32-wasip1 target installed successfully")


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
    # Try to checkout tag, if it fails try as commit hash
    try:
        run_cmd(f"git checkout {version}", cwd=repo_path, check=True)
    except subprocess.CalledProcessError:
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
    print(f"✓ Current version: {current_version}")

    return repo_path


def build_library(repo_path: Path, features: Optional[str] = None) -> Path:
    """Build libic_wasi_polyfill.a"""
    print(f"\n🔨 Building libic_wasi_polyfill.a...")
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
            print(f"⚠️  Found library file: {library_file.name}")
        else:
            raise FileNotFoundError(
                f"Library file not found in {target_dir}. "
                f"Build may have failed or library name is different."
            )

    print(f"✓ Library built successfully: {library_file}")
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
    print("\n🔍 Checking dependencies...")
    if not check_rust_installed():
        print("❌ Error: Rust is not installed")
        print("   Please visit https://rustup.rs/ to install Rust")
        sys.exit(1)

    if not check_cargo_installed():
        print("❌ Error: Cargo is not installed")
        print("   Please visit https://rustup.rs/ to install Rust (includes Cargo)")
        sys.exit(1)

    if not check_command_exists("git"):
        print("❌ Error: git is not installed")
        print("   Please install git")
        sys.exit(1)

    # Check and install wasm32-wasip1 target
    if not check_wasm32_target_installed():
        install_wasm32_target()
    else:
        print("✓ wasm32-wasip1 target is already installed")

    # Set work directory
    if args.work_dir:
        work_dir = Path(args.work_dir).resolve()
    else:
        work_dir = Path.home() / ".tmp_build_ic_wasi_polyfill"
    work_dir.mkdir(parents=True, exist_ok=True)

    # Clean option
    if args.clean and (work_dir / "ic-wasi-polyfill").exists():
        print(f"\n🧹 Cleaning work directory: {work_dir / 'ic-wasi-polyfill'}")
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
        print(f"✓ Library file copied to: {output_file}")
        print(f"  File size: {output_file.stat().st_size / 1024:.2f} KB")

        print("\n" + "=" * 70)
        print("✅ Build complete!")
        print("=" * 70)
        print(f"📦 Output file: {output_file}")
        print(f"📂 Source file location: {library_file}")
        if not args.work_dir:
            print(f"\n💡 Tip: Temporary files are located at {work_dir}")
            print(f"   To clean up, run: rm -rf {work_dir}")

    except subprocess.CalledProcessError as e:
        print(f"\n❌ Build failed: {e}")
        if e.stdout:
            print(f"Output: {e.stdout}")
        if e.stderr:
            print(f"Error: {e.stderr}")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()

