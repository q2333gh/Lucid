#!/usr/bin/env python3
"""
Post-processing pipeline for IC WASM artifacts.

Three-step pipeline:
  Step 1: wasi2ic     - Convert WASI WASM to IC-compatible format
  Step 2: wasm-opt    - Optimize WASM binary size
  Step 3: candid-ext  - Extract Candid interface definitions
  Step 4: copy-wasm   - Copy optimized WASM to example directories
"""

import shutil
import subprocess
import sys
import os
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
    examples: Optional[List[str]] = None,
) -> List[Path]:
    """
    Step 1: Convert all WASI WASM files to IC format using wasi2ic.

    Args:
        wasi2ic_tool: Path to wasi2ic binary
        bin_dir: Directory containing .wasm files
        examples: Optional list of example names to process (None = all)

    Returns:
        List of successfully converted _ic.wasm file paths
    """
    converted_files = []

    for wasm_file in bin_dir.glob("*.wasm"):
        # Skip already converted files
        if "_ic.wasm" in wasm_file.name:
            continue

        # Filter by examples if specified
        if examples is not None:
            # Extract example name from wasm filename (without .wasm extension)
            wasm_name = wasm_file.stem
            if wasm_name not in examples:
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
    examples: Optional[List[str]] = None,
) -> int:
    """
    Step 3: Extract Candid interfaces from _ic.wasm files.

    Args:
        bin_dir: Directory containing _ic.wasm files
        examples_dir: Root examples directory for .did output
        examples: Optional list of example names to process (None = all)

    Returns:
        Number of successfully extracted .did files
    """
    return extract_candid_for_examples(bin_dir, examples_dir, examples)


# =============================================================================
# Step 4: Copy WASM to Examples
# =============================================================================


def run_copy_wasm(
    bin_dir: Path, examples_dir: Path, examples: Optional[List[str]] = None
) -> int:
    """
    Step 4: Copy optimized WASM files to their corresponding example directories.

    Args:
        bin_dir: Directory containing _ic.wasm files
        examples_dir: Root examples directory
        examples: Optional list of example names to process (None = all)

    Returns:
        Number of successfully copied files
    """
    copied_count = 0

    for wasm_file in bin_dir.glob("*_ic.wasm"):
        filename = wasm_file.name
        canister_name = filename.replace("_ic.wasm", "")

        # Filter by examples if specified
        if examples is not None:
            if canister_name not in examples:
                continue

        # Find matching example directory
        target_example_dir = None
        for d in examples_dir.iterdir():
            if not d.is_dir():
                continue
            if d.name == canister_name:
                target_example_dir = d
                break
            if d.name == "hello_lucid" and canister_name == "greet":
                target_example_dir = d
                break

        if target_example_dir:
            dest_path = target_example_dir / wasm_file.name
            try:
                shutil.copy2(wasm_file, dest_path)
                print(f"   Copied {wasm_file.name} -> {dest_path}")
                copied_count += 1
            except Exception as e:
                print(f"   Error copying {wasm_file.name}: {e}")

    return copied_count


# =============================================================================
# Pipeline Orchestrator
# =============================================================================


class PostProcessStats:
    """Statistics for post-processing pipeline."""

    def __init__(self):
        self.wasi2ic_count = 0
        self.wasm_opt_count = 0
        self.candid_count = 0
        self.copied_count = 0

    def __repr__(self):
        return (
            f"wasi2ic: {self.wasi2ic_count}, "
            f"wasm-opt: {self.wasm_opt_count}, "
            f"candid: {self.candid_count}, "
            f"copied: {self.copied_count}"
        )

    def as_dict(self) -> dict:
        return {
            "converted": self.wasi2ic_count,
            "optimized": self.wasm_opt_count,
            "candid": self.candid_count,
            "copied": self.copied_count,
        }


def post_process_wasm_files(
    bin_dir: Path,
    wasi2ic_tool: Path,
    examples_dir: Path,
    optimization_level: str = "-Oz",
    examples: Optional[List[str]] = None,
) -> dict:
    """
    Execute the complete post-processing pipeline.

    Pipeline:
      Step 1: wasi2ic     - WASI -> IC conversion
      Step 2: wasm-opt    - Binary optimization
      Step 3: candid-ext  - Interface extraction
      Step 4: copy-wasm   - Copy optimized WASM to example directories

    Args:
        bin_dir: Directory containing .wasm files
        wasi2ic_tool: Path to wasi2ic tool
        examples_dir: Root examples directory
        optimization_level: wasm-opt level
        examples: Optional list of example names to process (None = all)

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
    print(" Step 1/4: wasi2ic (WASI -> IC conversion)")
    converted_files = run_wasi2ic(wasi2ic_tool, bin_dir, examples)
    stats.wasi2ic_count = len(converted_files)

    # Step 2: wasm-opt
    if converted_files:
        print(" Step 2/4: wasm-opt (binary optimization)")
        stats.wasm_opt_count = run_wasm_opt(converted_files, optimization_level)

    # Step 3: candid-extractor
    print(" Step 3/4: candid-extractor (interface extraction)")
    stats.candid_count = run_candid_extractor(bin_dir, examples_dir, examples)

    # Step 4: Copy WASM
    print(" Step 4/4: Copy optimized WASM to examples")
    stats.copied_count = run_copy_wasm(bin_dir, examples_dir, examples)

    return stats.as_dict()
