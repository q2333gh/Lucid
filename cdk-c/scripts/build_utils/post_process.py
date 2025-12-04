#!/usr/bin/env python3
"""
Post-processing pipeline for IC WASM artifacts.

Three-step pipeline:
  Step 1: wasi2ic     - Convert WASI WASM to IC-compatible format
  Step 2: wasm-opt    - Optimize WASM binary size
  Step 3: candid-ext  - Extract Candid interface definitions
"""

import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional

# Handle imports for both package and direct loading
_script_dir = Path(__file__).parent.resolve()
if str(_script_dir) not in sys.path:
    sys.path.insert(0, str(_script_dir))

try:
    from .wasm_opt import optimize_wasm
    from .extract_candid import extract_candid_for_examples
except ImportError:
    from wasm_opt import optimize_wasm
    from extract_candid import extract_candid_for_examples


# =============================================================================
# Step 1: wasi2ic - WASI to IC Conversion
# =============================================================================


def run_wasi2ic(
    wasi2ic_tool: Path,
    bin_dir: Path,
) -> List[Path]:
    """
    Step 1: Convert all WASI WASM files to IC format using wasi2ic.

    Args:
        wasi2ic_tool: Path to wasi2ic binary
        bin_dir: Directory containing .wasm files

    Returns:
        List of successfully converted _ic.wasm file paths
    """
    converted_files = []

    for wasm_file in bin_dir.glob("*.wasm"):
        # Skip already converted files
        if "_ic.wasm" in wasm_file.name:
            continue

        output_wasm = bin_dir / f"{wasm_file.stem}_ic.wasm"
        print(f"   {wasm_file.name} -> {output_wasm.name}")

        try:
            subprocess.run(
                [str(wasi2ic_tool), str(wasm_file), str(output_wasm)],
                check=True,
                capture_output=True,
            )
            converted_files.append(output_wasm)
        except subprocess.CalledProcessError as e:
            stderr = e.stderr.decode() if e.stderr else "unknown error"
            print(f"   Error: {stderr}")

    return converted_files


# =============================================================================
# Step 2: wasm-opt - Binary Optimization
# =============================================================================


def run_wasm_opt(
    wasm_files: List[Path],
    optimization_level: str = "-Oz",
) -> int:
    """
    Step 2: Optimize WASM files using wasm-opt.

    Args:
        wasm_files: List of WASM files to optimize
        optimization_level: -Oz for size, -O4 for speed

    Returns:
        Number of successfully optimized files
    """
    optimized_count = 0

    for wasm_file in wasm_files:
        if not wasm_file.exists():
            continue

        temp_file = wasm_file.parent / f"{wasm_file.stem}_opt_temp.wasm"
        print(f"   {wasm_file.name}")

        if optimize_wasm(wasm_file, temp_file, optimization_level):
            shutil.move(str(temp_file), str(wasm_file))
            optimized_count += 1
        else:
            if temp_file.exists():
                temp_file.unlink()

    return optimized_count


# =============================================================================
# Step 3: candid-extractor - Interface Extraction
# =============================================================================


def run_candid_extractor(
    bin_dir: Path,
    examples_dir: Path,
) -> int:
    """
    Step 3: Extract Candid interfaces from _ic.wasm files.

    Args:
        bin_dir: Directory containing _ic.wasm files
        examples_dir: Root examples directory for .did output

    Returns:
        Number of successfully extracted .did files
    """
    return extract_candid_for_examples(bin_dir, examples_dir)


# =============================================================================
# Pipeline Orchestrator
# =============================================================================


class PostProcessStats:
    """Statistics for post-processing pipeline."""

    def __init__(self):
        self.wasi2ic_count = 0
        self.wasm_opt_count = 0
        self.candid_count = 0

    def __repr__(self):
        return (
            f"wasi2ic: {self.wasi2ic_count}, "
            f"wasm-opt: {self.wasm_opt_count}, "
            f"candid: {self.candid_count}"
        )

    def as_dict(self) -> dict:
        return {
            "converted": self.wasi2ic_count,
            "optimized": self.wasm_opt_count,
            "candid": self.candid_count,
        }


def post_process_wasm_files(
    bin_dir: Path,
    wasi2ic_tool: Path,
    examples_dir: Path,
    optimization_level: str = "-Oz",
) -> dict:
    """
    Execute the complete post-processing pipeline.

    Pipeline:
      Step 1: wasi2ic     - WASI -> IC conversion
      Step 2: wasm-opt    - Binary optimization
      Step 3: candid-ext  - Interface extraction

    Args:
        bin_dir: Directory containing .wasm files
        wasi2ic_tool: Path to wasi2ic tool
        examples_dir: Root examples directory
        optimization_level: wasm-opt level

    Returns:
        Dict with counts for each step
    """
    stats = PostProcessStats()

    if not bin_dir.exists():
        print(f" Warning: Bin directory not found: {bin_dir}")
        return stats.as_dict()

    if not wasi2ic_tool.exists():
        print(f" Warning: wasi2ic tool not found: {wasi2ic_tool}")
        return stats.as_dict()

    # Step 1: wasi2ic
    print(" Step 1/3: wasi2ic (WASI -> IC conversion)")
    converted_files = run_wasi2ic(wasi2ic_tool, bin_dir)
    stats.wasi2ic_count = len(converted_files)

    # Step 2: wasm-opt
    if converted_files:
        print(" Step 2/3: wasm-opt (binary optimization)")
        stats.wasm_opt_count = run_wasm_opt(converted_files, optimization_level)

    # Step 3: candid-extractor
    print(" Step 3/3: candid-extractor (interface extraction)")
    stats.candid_count = run_candid_extractor(bin_dir, examples_dir)

    return stats.as_dict()
