#!/usr/bin/env python3
"""
Post-processing utilities for WASM build artifacts.

Handles:
- wasi2ic conversion
- wasm-opt optimization
- Candid interface extraction
"""

import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional

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


def convert_wasi_to_ic(
    wasi2ic_tool: Path,
    input_wasm: Path,
    output_wasm: Path,
) -> bool:
    """
    Convert WASI WASM to IC-compatible WASM using wasi2ic.

    Args:
        wasi2ic_tool: Path to wasi2ic binary
        input_wasm: Path to input WASM file
        output_wasm: Path to output _ic.wasm file

    Returns:
        True if conversion succeeded, False otherwise
    """
    try:
        subprocess.run(
            [str(wasi2ic_tool), str(input_wasm), str(output_wasm)],
            check=True,
            capture_output=True,
        )
        return True
    except subprocess.CalledProcessError as e:
        stderr = e.stderr.decode() if e.stderr else "unknown error"
        print(f" Error: wasi2ic failed: {stderr}")
        return False

# wasi2ic and wasm-opt and candid interface extraction
def post_process_wasm_files(
    bin_dir: Path,
    wasi2ic_tool: Path,
    examples_dir: Path,
    optimization_level: str = "-Oz",
) -> dict:
    """
    Post-process all WASM files in bin_dir.

    Steps for each .wasm file:
    1. Convert to IC format using wasi2ic -> *_ic.wasm
    2. Optimize using wasm-opt (in-place)
    3. Extract Candid interface -> *.did

    Args:
        bin_dir: Directory containing .wasm files
        wasi2ic_tool: Path to wasi2ic tool
        examples_dir: Root examples directory for .did output
        optimization_level: wasm-opt optimization level

    Returns:
        Dict with counts: {'converted': N, 'optimized': N, 'candid': N}
    """
    stats = {"converted": 0, "optimized": 0, "candid": 0}

    if not bin_dir.exists():
        print(f" Warning: Bin directory not found: {bin_dir}")
        return stats

    if not wasi2ic_tool.exists():
        print(f" Warning: wasi2ic tool not found: {wasi2ic_tool}")
        return stats

    # Step 1 & 2: Convert and optimize each WASM file
    for wasm_file in bin_dir.glob("*.wasm"):
        # Skip already converted files
        if "_ic.wasm" in wasm_file.name:
            continue

        output_wasm = bin_dir / f"{wasm_file.stem}_ic.wasm"
        print(f" Converting {wasm_file.name} -> {output_wasm.name}...")

        if convert_wasi_to_ic(wasi2ic_tool, wasm_file, output_wasm):
            print(f" Success: {output_wasm.name}")
            stats["converted"] += 1

            # Optimize the IC-converted WASM file
            print(f" Optimizing {output_wasm.name}...")
            temp_optimized = bin_dir / f"{output_wasm.stem}_opt_temp.wasm"

            if optimize_wasm(output_wasm, temp_optimized, optimization_level):
                # Replace original with optimized version
                shutil.move(str(temp_optimized), str(output_wasm))
                print(f" Optimized: {output_wasm.name}")
                stats["optimized"] += 1
            else:
                # Clean up temp file if it exists
                if temp_optimized.exists():
                    temp_optimized.unlink()
                print(f" Using unoptimized: {output_wasm.name}")
        else:
            print(f" Failed to convert {wasm_file.name}")

    # Step 3: Extract Candid interfaces
    print("\n Extracting Candid interfaces...")
    stats["candid"] = extract_candid_for_examples(bin_dir, examples_dir)

    return stats
