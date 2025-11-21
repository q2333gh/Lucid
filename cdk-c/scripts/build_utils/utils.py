#!/usr/bin/env python3
"""
Utility functions for IC C SDK build system
Command execution, file finding, and other helper functions
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path
from typing import Optional, Tuple

from .config import IC_WASI_POLYFILL_COMMIT, WASI2IC_COMMIT


def run_command(cmd, description="", cwd=None, show_stderr=True):
    """
    Execute command and handle errors
    
    Args:
        cmd: Command to execute (list of strings)
        description: Description of the command for output
        cwd: Working directory for command execution
        show_stderr: Whether to show stderr output
    
    Returns:
        bool: True if command succeeded, False otherwise
    """
    print(f"🔨 {description or ' '.join(cmd)}")
    try:
        result = subprocess.run(
            cmd, 
            check=True, 
            capture_output=True, 
            text=True,
            cwd=cwd
        )
        if result.stdout:
            print(result.stdout)
        # Show stderr for compiler messages (like #pragma message and #warning)
        if show_stderr and result.stderr:
            print(result.stderr, end='')
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ Error: {e}")
        if e.stderr:
            print(f"Error details: {e.stderr}")
        return False
    except FileNotFoundError:
        print(f"❌ Command not found: {cmd[0]}")
        print(f"   Please ensure the corresponding compiler is installed")
        return False


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
    Find wasi2ic tool
    
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
            return env_path
    
    # Method 2: Scripts directory (if built locally)
    scripts_wasi2ic = scripts_dir / "wasi2ic"
    if scripts_wasi2ic.exists():
        return scripts_wasi2ic
    
    # Method 3: Common installation locations
    # Check if wasi2ic is in PATH
    wasi2ic_in_path = shutil.which("wasi2ic")
    if wasi2ic_in_path:
        return Path(wasi2ic_in_path)
    
    # Default to scripts directory (will be checked/used when needed)
    return scripts_wasi2ic


def ensure_polyfill_library(scripts_dir: Path, build_polyfill_script: Path) -> Optional[Path]:
    """
    Ensure libic_wasi_polyfill.a exists, build it if needed
    
    Args:
        scripts_dir: Scripts directory path
        build_polyfill_script: Path to build script
    
    Returns:
        Path to polyfill library if successful, None otherwise
    """
    # Method 1: Check scripts directory (preferred)
    scripts_polyfill = scripts_dir / "libic_wasi_polyfill.a"
    if scripts_polyfill.exists():
        print(f"\n📦 Using IC WASI Polyfill library:")
        print(f"   Path: {scripts_polyfill.resolve()}")
        
        return scripts_polyfill
    
    # Method 2: Check environment variable override
    env_polyfill = os.environ.get("IC_WASI_POLYFILL_PATH")
    if env_polyfill:
        env_path = Path(env_polyfill)
        if env_path.exists():
            print(f"\n📦 Using IC WASI Polyfill library (from IC_WASI_POLYFILL_PATH):")
            print(f"   Path: {env_path.resolve()}")
            
            return env_path
    
    # If neither exists, try to build it
    print(f"\n⚠️  libic_wasi_polyfill.a not found")
    
    print(f"   Attempting to build it using build_libic_wasi_polyfill.py...")
    
    if not build_polyfill_script.exists():
        print(f"❌ Build script not found: {build_polyfill_script}")
        print(f"   Please run build_libic_wasi_polyfill.py manually")
        print(f"   Expected commit: {IC_WASI_POLYFILL_COMMIT}")
        return None
    
    # Run the build script (should use locked commit)
    cmd = [
        sys.executable, 
        str(build_polyfill_script), 
        "--output-dir", str(scripts_dir),
        "--version", IC_WASI_POLYFILL_COMMIT
    ]
    if not run_command(cmd, f"Building libic_wasi_polyfill.a"):
        print(f"❌ Failed to build libic_wasi_polyfill.a")
        print(f"   Expected commit: {IC_WASI_POLYFILL_COMMIT}")
        return None
    
    if not scripts_polyfill.exists():
        print(f"❌ libic_wasi_polyfill.a still not found after build attempt")
        return None
    
    print(f"✓ libic_wasi_polyfill.a is ready")
    print(f"\n📦 Using IC WASI Polyfill library (newly built):")
    print(f"   Path: {scripts_polyfill.resolve()}")
    
    return scripts_polyfill


def ensure_wasi2ic_tool(scripts_dir: Path, build_wasi2ic_script: Path) -> Optional[Path]:
    """
    Ensure wasi2ic tool exists, build it if needed
    
    Args:
        scripts_dir: Scripts directory path
        build_wasi2ic_script: Path to build script
    
    Returns:
        Path to wasi2ic tool if successful, None otherwise
    """
    # Re-check wasi2ic location (may have been set via environment)
    wasi2ic = find_wasi2ic_tool(scripts_dir)
    
    # Method 1: Check if already exists
    if wasi2ic.exists():
        print(f"\n🔧 Using wasi2ic tool:")
        print(f"   Path: {wasi2ic.resolve()}")
        
        return wasi2ic
    
    # Method 2: Check environment variable override
    env_wasi2ic = os.environ.get("WASI2IC_PATH")
    if env_wasi2ic:
        env_path = Path(env_wasi2ic)
        if env_path.exists():
            print(f"\n🔧 Using wasi2ic tool (from WASI2IC_PATH):")
            print(f"   Path: {env_path.resolve()}")
            
            return env_path
    
    # Method 3: Check if wasi2ic is in PATH
    wasi2ic_in_path = shutil.which("wasi2ic")
    if wasi2ic_in_path:
        wasi2ic = Path(wasi2ic_in_path)
        print(f"\n🔧 Using wasi2ic tool (from PATH):")
        print(f"   Path: {wasi2ic.resolve()}")
        
        return wasi2ic
    
    # If not found, try to build it
    print(f"\n⚠️  wasi2ic tool not found")
    
    print(f"   Attempting to build it using build_wasi2ic.py...")
    
    if not build_wasi2ic_script.exists():
        print(f"❌ Build script not found: {build_wasi2ic_script}")
        print(f"   Please run build_wasi2ic.py manually")
        print(f"   Expected commit: {WASI2IC_COMMIT}")
        return None
    
    # Run the build script (should use locked commit)
    cmd = [
        sys.executable, 
        str(build_wasi2ic_script), 
        "--output-dir", str(scripts_dir),
        "--version", WASI2IC_COMMIT
    ]
    if not run_command(cmd, f"Building wasi2ic tool"):
        print(f"❌ Failed to build wasi2ic tool")
        print(f"   Expected commit: {WASI2IC_COMMIT}")
        return None
    
    wasi2ic = scripts_dir / "wasi2ic"
    if not wasi2ic.exists():
        print(f"❌ wasi2ic tool still not found after build attempt")
        return None
    
    print(f"✓ wasi2ic tool is ready")
    print(f"\n🔧 Using wasi2ic tool (newly built):")
    print(f"   Path: {wasi2ic.resolve()}")
    
    return wasi2ic


def check_source_files_exist(src_dir: Path, source_files: list) -> Tuple[bool, list]:
    """
    Check if all source files exist
    
    Args:
        src_dir: Source directory path
        source_files: List of source file names
    
    Returns:
        Tuple of (all_exist: bool, missing_files: list)
    """
    missing_files = []
    for src in source_files:
        if not (src_dir / src).exists():
            missing_files.append(src)
    
    return (len(missing_files) == 0, missing_files)


def needs_rebuild(lib_path: Path, src_dir: Path, source_files: list, target_platform: str, build_dir: Path) -> bool:
    """
    Check if library needs to be rebuilt
    
    Args:
        lib_path: Path to library file
        src_dir: Source directory path
        source_files: List of source file names
        target_platform: Target platform ("native" or "wasi")
        build_dir: Build directory path
    
    Returns:
        True if rebuild is needed, False otherwise
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
    
    Returns:
        Path to wasm-opt tool if found, None otherwise
    """
    wasm_opt_in_path = shutil.which("wasm-opt")
    if wasm_opt_in_path:
        return Path(wasm_opt_in_path)
    return None


def optimize_wasm(wasm_file: Path, optimized_file: Path, optimization_level: str = "-Oz") -> bool:
    """
    Optimize WASM file using wasm-opt
    
    Args:
        wasm_file: Path to input WASM file
        optimized_file: Path to output optimized WASM file
        optimization_level: Optimization level (default: "-Oz" for size optimization)
    
    Returns:
        True if optimization succeeded, False otherwise
    """
    wasm_opt = find_wasm_opt()
    if not wasm_opt:
        print(f"  ⚠️  wasm-opt not found in PATH, skipping optimization")
        print(f"     Install binaryen package: sudo apt install binaryen")
        print(f"     Or: brew install binaryen (on macOS)")
        return False
    
    if not wasm_file.exists():
        print(f"  ❌ Input WASM file not found: {wasm_file}")
        return False
    
    # Run wasm-opt with optimization flags
    # -Oz: optimize for size (most aggressive)
    # --strip-debug: remove debug information
    cmd = [str(wasm_opt), optimization_level, "--strip-debug", str(wasm_file), "-o", str(optimized_file)]
    
    if run_command(cmd, f"Optimizing {wasm_file.name} with wasm-opt"):
        if optimized_file.exists():
            # Show file size comparison
            original_size = wasm_file.stat().st_size
            optimized_size = optimized_file.stat().st_size
            reduction = ((original_size - optimized_size) / original_size) * 100
            print(f"  ✅ Optimized: {wasm_file.name} ({original_size:,} bytes) -> {optimized_file.name} ({optimized_size:,} bytes, {reduction:.1f}% reduction)")
            return True
        else:
            print(f"  ⚠️  Optimization completed but output file not found: {optimized_file}")
            return False
    else:
        print(f"  ⚠️  wasm-opt optimization failed, but original WASM file is available: {wasm_file}")
        return False

