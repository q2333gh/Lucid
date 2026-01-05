#!/usr/bin/env python3
"""
IC C SDK Build Script

Build pipeline for IC (Internet Computer) C SDK projects.

    Phases:
      1. Toolchain    - WASI SDK setup
      2. IC Tools     - polyfill library + wasi2ic tool
      3. Build        - CMake configure + compile
      4. Post-process - wasi2ic -> wasm-opt -> candid-extractor -> dfx.json

    Usage:
    python build.py            # Native build (with tests)
    python build.py --icwasm   # IC WASM canister build (with post-processing)
"""

import argparse
import multiprocessing
import shutil
import subprocess
import sys
import os
from pathlib import Path
from typing import List, Optional

# =============================================================================
# Module Setup
# =============================================================================

_ROOT_DIR = Path(__file__).parent.resolve()
_BUILD_UTILS = _ROOT_DIR / "cdk-c/scripts/build_utils"
_SCRIPTS_DIR = _ROOT_DIR / "cdk-c/scripts"

if str(_BUILD_UTILS) not in sys.path:
    sys.path.insert(0, str(_BUILD_UTILS))

# Ensure scripts directory is in path for generate_dfx
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.append(str(_SCRIPTS_DIR))

from wasi_sdk import ensure_wasi_sdk
from builders import ensure_polyfill_library, ensure_wasi2ic_tool
from post_process import (
    post_process_wasm_files,
    run_wasi2ic,
    run_wasm_opt,
    run_candid_extractor,
)
from project_manager import create_new_project

# Try to import generate_dfx, but don't fail if it's not there yet
try:
    # This import relies on sys.path modification above.
    # We add type: ignore to silence static analysis tools that don't see the path change.
    import generate_dfx as generate_dfx_json_module  # type: ignore
except ImportError:
    # Fallback if script is missing or path issue
    generate_dfx_json_module = None
    print("Warning: generate_dfx module not found, skipping dfx.json generation.")


# =============================================================================
# Phase 1: Toolchain - WASI SDK
# =============================================================================


def setup_wasi_sdk() -> tuple:
    """
    Phase 1: Verify and setup WASI SDK toolchain.

    Returns:
        (sdk_path, toolchain_file) or exits on failure
    """
    print("[Phase 1] Toolchain: WASI SDK")

    sdk_path, toolchain_file = ensure_wasi_sdk()
    if not sdk_path or not toolchain_file:
        print(" Error: WASI SDK not found.")
        print(" Set WASI_SDK_ROOT environment variable.")
        sys.exit(1)

    print(f" SDK: {sdk_path}")
    print(f" Toolchain: {toolchain_file.name}")
    return sdk_path, toolchain_file


# =============================================================================
# Phase 2: IC Tools - Polyfill + wasi2ic
# =============================================================================


def setup_ic_tools(build_lib_dir: Path) -> tuple:
    """
    Phase 2: Ensure IC-specific conversion tools are available.

    - libic_wasi_polyfill.a: WASI polyfill for IC environment
    - wasi2ic: WASI to IC WASM converter

    Returns:
        (polyfill_lib_path, wasi2ic_tool_path)
    """
    print("\n[Phase 2] IC Tools: polyfill + wasi2ic")

    # Polyfill library (required for linking)
    polyfill_lib = ensure_polyfill_library(build_lib_dir)
    if not polyfill_lib or not polyfill_lib.exists():
        print(" Error: Failed to build polyfill library.")
        sys.exit(1)
    print(f" Polyfill: {polyfill_lib.name}")

    # wasi2ic tool (required for post-processing)
    wasi2ic_tool = ensure_wasi2ic_tool(build_lib_dir)
    if not wasi2ic_tool or not wasi2ic_tool.exists():
        print(" Warning: wasi2ic not available, post-processing will be skipped.")
        wasi2ic_tool = None
    else:
        print(f" wasi2ic: {wasi2ic_tool.name}")

    return polyfill_lib, wasi2ic_tool


# =============================================================================
# Phase 3: Build - CMake Configure + Compile
# =============================================================================


def run_cmake_build(
    build_dir: Path,
    wasi: bool,
    cmake_extra_args: list,
) -> None:
    """
    Phase 3: Configure and build the project.

    Args:
        build_dir: Build output directory
        wasi: True for WASI target, False for native
        cmake_extra_args: Additional CMake arguments
    """
    print(f"\n[Phase 3] Build: CMake + {'Ninja' if shutil.which('ninja') else 'Make'}")
    print(f" Directory: {build_dir}")

    # CMake configure
    use_ninja = shutil.which("ninja") is not None
    cmake_args = ["cmake", "..", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]

    if use_ninja:
        cmake_args.extend(["-G", "Ninja"])

    if wasi:
        cmake_args.extend(["-DCMAKE_BUILD_TYPE=MinSizeRel"])
    else:
        # Use Clang for native builds (better clang-tidy integration)
        if shutil.which("clang"):
            cmake_args.append("-DCMAKE_C_COMPILER=clang")
        cmake_args.extend(["-DBUILD_CDK_TESTS=ON", "-DC_CANDID_BUILD_TESTS=ON"])

    cmake_args.extend(cmake_extra_args)

    print(" Configuring...")
    subprocess.run(cmake_args, cwd=build_dir, check=True)

    # Build
    print(" Compiling...")
    if use_ninja:
        jobs = multiprocessing.cpu_count()
        subprocess.run(["ninja", "-j", str(jobs)], cwd=build_dir, check=True)
    else:
        subprocess.run(["cmake", "--build", "."], cwd=build_dir, check=True)

    # Copy compile_commands.json for IDE
    compile_commands = build_dir / "compile_commands.json"
    if compile_commands.exists():
        shutil.copy(compile_commands, _ROOT_DIR / "compile_commands.json")


# =============================================================================
# Phase 4: Post-processing - wasi2ic + wasm-opt + candid
# =============================================================================


def run_post_processing(
    bin_dir: Path,
    wasi2ic_tool: Path,
    examples_dir: Path,
    examples: Optional[List[str]] = None,
) -> None:
    """
    Phase 4: Post-process WASM artifacts.

    Pipeline:
      Step 1: wasi2ic         - Convert WASI WASM to IC format
      Step 2: wasm-opt        - Optimize binary size (-Oz)
      Step 3: candid-extractor - Extract .did interface files
      Step 4: generate_dfx    - Generate dfx.json for examples

    Args:
        bin_dir: Directory containing built .wasm files
        wasi2ic_tool: Path to wasi2ic binary
        examples_dir: Examples directory for .did output
        examples: Optional list of example names to process (None = all)
    """
    # TODO if compile successed last time and no changes, skip this step
    print("\n[Phase 4] Post-processing: wasi2ic -> wasm-opt -> candid -> dfx.json")

    if not bin_dir.exists():
        print(" Warning: No bin directory found, skipping.")
        return

    stats = post_process_wasm_files(
        bin_dir=bin_dir,
        wasi2ic_tool=wasi2ic_tool,
        examples_dir=examples_dir,
        optimization_level="-Oz",
        examples=examples,
    )

    print(
        f"\n Summary: {stats['converted']} converted, "
        f"{stats['optimized']} optimized, "
        f"{stats['candid']} .did extracted"
    )

    # Phase 4, Step 4: Generate dfx.json for examples
    if generate_dfx_json_module:
        generate_dfx_json_module.auto_generate_dfx(bin_dir, examples_dir, examples)


# =============================================================================
# Main Build Function
# =============================================================================


def build(wasi: bool = False, examples: Optional[List[str]] = None) -> None:
    """
    Main build orchestrator.

    WASI build phases:
      1. Toolchain    - WASI SDK
      2. IC Tools     - polyfill + wasi2ic
      3. Build        - CMake + compile
      4. Post-process - wasi2ic -> wasm-opt -> candid -> dfx.json

    Native build phases:
      3. Build        - CMake + compile + tests
    """
    # Directory setup
    build_dir = _ROOT_DIR / ("build-wasi" if wasi else "build")
    build_lib_dir = _ROOT_DIR / "build_lib"
    examples_dir = _ROOT_DIR / "examples"

    build_dir.mkdir(exist_ok=True)
    build_lib_dir.mkdir(exist_ok=True)

    cmake_extra_args = []
    wasi2ic_tool = None

    if wasi:
        # Phase 1: Toolchain
        sdk_path, toolchain_file = setup_wasi_sdk()
        cmake_extra_args.extend(
            [
                f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
                f"-DWASI_SDK_PREFIX={sdk_path}",
            ]
        )

        # Phase 2: IC Tools
        polyfill_lib, wasi2ic_tool = setup_ic_tools(build_lib_dir)
        cmake_extra_args.append(f"-DIC_WASI_POLYFILL_PATH={polyfill_lib}")

    else:
        print(" Target: Native (Host)")

    # Handle examples
    if examples:
        # Sanitize examples input (remove 'examples/' prefix if present)
        sanitized_examples = []
        for ex in examples:
            clean_ex = ex.rstrip("/").split("/")[-1]
            sanitized_examples.append(clean_ex)

        examples_list = ";".join(sanitized_examples)
        cmake_extra_args.append(f"-DBUILD_EXAMPLES={examples_list}")

    # Phase 3: Build
    run_cmake_build(build_dir, wasi, cmake_extra_args)

    # Native: run tests
    if not wasi:
        print("\n[Running Tests]")
        subprocess.run(
            ["ctest", "--output-on-failure"],
            cwd=build_dir,
            check=False,
        )

    # Phase 4: Post-processing (WASI only)
    if wasi and wasi2ic_tool:
        # Pass sanitized examples to post-processing
        sanitized_examples = None
        if examples:
            sanitized_examples = []
            for ex in examples:
                clean_ex = ex.rstrip("/").split("/")[-1]
                sanitized_examples.append(clean_ex)

        run_post_processing(
            bin_dir=build_dir / "bin",
            wasi2ic_tool=wasi2ic_tool,
            examples_dir=examples_dir,
            examples=sanitized_examples,
        )

    print("\n✓ Build complete.")


# =============================================================================
# Entry Point
# =============================================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Build IC C SDK project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python build.py            Native build (with tests)
  python build.py --icwasm   IC WASM canister build (with post-processing)
  python build.py --icwasm --examples hello_lucid inter-canister-call
  python build.py --new my_canister  Create a new minimal project
        """,
    )
    # TODO if any step goes err,early exit with RED COLOR FATAL MESSAGE
    parser.add_argument(
        "--icwasm",
        action="store_true",
        help="Build for IC WASM canister platform",
    )

    parser.add_argument(
        "--examples",
        nargs="+",
        help="Specify which examples to build (default: hello_lucid). Usage: --examples proj1 proj2 ",
        default=None,
    )

    parser.add_argument(
        "--new",
        help="Create a new minimal project with the given name",
        type=str,
        default=None,
    )

    args = parser.parse_args()

    if args.new:
        create_new_project(args.new, _ROOT_DIR)
        sys.exit(0)

    try:
        build(wasi=args.icwasm, examples=args.examples)
    except subprocess.CalledProcessError as e:
        print(f"\n✗ Build failed: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n✗ Build interrupted.")
        sys.exit(130)
