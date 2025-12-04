#!/usr/bin/env python3
"""
WASM optimization utilities using wasm-opt (binaryen).

Provides functions to optimize WASM files for size or speed.
"""

import shutil
import subprocess
from pathlib import Path
from typing import Optional


def find_wasm_opt() -> Optional[Path]:
    """Find wasm-opt tool in PATH."""
    wasm_opt = shutil.which("wasm-opt")
    return Path(wasm_opt) if wasm_opt else None


def optimize_wasm(
    input_file: Path,
    output_file: Path,
    optimization_level: str = "-Oz",
) -> bool:
    """
    Optimize WASM file using wasm-opt.

    Args:
        input_file: Path to input WASM file
        output_file: Path to output optimized WASM file
        optimization_level: Optimization level
            -Oz: Optimize for size (default)
            -O4: Optimize for speed

    Returns:
        True if optimization succeeded, False otherwise
    """
    wasm_opt = find_wasm_opt()
    if not wasm_opt:
        print(" Warning: wasm-opt not found in PATH, skipping optimization")
        print(
            "   Install binaryen: sudo apt install binaryen "
            "(or brew install binaryen)"
        )
        return False

    if not input_file.exists():
        print(f" Error: Input WASM file not found: {input_file}")
        return False

    original_size = input_file.stat().st_size

    try:
        subprocess.run(
            [
                str(wasm_opt),
                optimization_level,
                "--strip-debug",
                str(input_file),
                "-o",
                str(output_file),
            ],
            check=True,
            capture_output=True,
        )

        if output_file.exists():
            optimized_size = output_file.stat().st_size
            reduction = ((original_size - optimized_size) / original_size) * 100
            print(
                f" Optimized: {original_size:,} -> {optimized_size:,} bytes "
                f"({reduction:.1f}% reduction)"
            )
            return True
        else:
            print(" Warning: wasm-opt completed but output file not found")
            return False

    except subprocess.CalledProcessError as e:
        stderr = e.stderr.decode() if e.stderr else "unknown error"
        print(f" Warning: wasm-opt failed: {stderr}")
        return False
