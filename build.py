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
    python build.py            # Show help
    python build.py --test     # Native build with tests
    python build.py --icwasm   # IC WASM canister build (with post-processing)
"""

import argparse
import multiprocessing
import shutil
import subprocess
import sys
import os
import urllib.request
import tarfile
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

from wasi_sdk import ensure_wasi_sdk
from builders import ensure_polyfill_library, ensure_wasi2ic_tool
from post_process import (
    post_process_wasm_files,
    run_wasi2ic,
    run_wasm_opt,
    run_candid_extractor,
)
from project_manager import create_new_project
from dfxjson import generate_dfx as generate_dfx_json_module


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

# Default release tag for downloading pre-built artifacts
_DEFAULT_RELEASE_TAG = "polyfill_and_wasi2ic"
_DEFAULT_RELEASE_BASE_URL = (
    f"https://github.com/q2333gh/Lucid/releases/download/{_DEFAULT_RELEASE_TAG}"
)


def _download_release_artifact(url: str, output_path: Path) -> bool:
    """
    Download and extract a release artifact from GitHub.

    Args:
        url: URL to the tar.gz artifact
        output_path: Directory to extract the artifact to

    Returns:
        True if download and extraction succeeded, False otherwise
    """
    temp_file = None
    try:
        print(f" Downloading {url}...")
        output_path.mkdir(parents=True, exist_ok=True)

        # Download to temporary file
        temp_file = output_path / ".tmp_download.tar.gz"
        urllib.request.urlretrieve(url, temp_file)

        # Extract tar.gz
        with tarfile.open(temp_file, "r:gz") as tar:
            tar.extractall(path=output_path)

        # Clean up temp file
        temp_file.unlink()

        print(f" Successfully downloaded and extracted to {output_path}")
        return True
    except Exception as e:
        print(f" Warning: Failed to download {url}: {e}")
        if temp_file and temp_file.exists():
            temp_file.unlink()
        return False


def _try_download_ic_tools(build_lib_dir: Path, use_release: bool = True) -> tuple:
    """
    Try to download IC tools from GitHub release.

    Args:
        build_lib_dir: Directory to place downloaded artifacts
        use_release: If True, attempt to download from release (default: True)

    Returns:
        (polyfill_lib_path, wasi2ic_tool_path) or (None, None) if download failed
    """
    if not use_release:
        return None, None

    polyfill_url = f"{_DEFAULT_RELEASE_BASE_URL}/libic_wasi_polyfill.a.tar.gz"
    wasi2ic_url = f"{_DEFAULT_RELEASE_BASE_URL}/wasi2ic-linux-amd64.tar.gz"

    polyfill_lib = build_lib_dir / "libic_wasi_polyfill.a"
    wasi2ic_tool = build_lib_dir / "wasi2ic"

    # Check if already exists
    if polyfill_lib.exists() and wasi2ic_tool.exists():
        return polyfill_lib, wasi2ic_tool

    # Try downloading polyfill
    if not polyfill_lib.exists():
        if not _download_release_artifact(polyfill_url, build_lib_dir):
            return None, None
        if not polyfill_lib.exists():
            print(" Warning: libic_wasi_polyfill.a not found after download")
            return None, None

    # Try downloading wasi2ic
    if not wasi2ic_tool.exists():
        if not _download_release_artifact(wasi2ic_url, build_lib_dir):
            return None, None
        # Extract wasi2ic from the tar.gz (it might be named wasi2ic-linux-amd64)
        extracted = build_lib_dir / "wasi2ic-linux-amd64"
        if extracted.exists():
            extracted.rename(wasi2ic_tool)
        if not wasi2ic_tool.exists():
            print(" Warning: wasi2ic not found after download")
            return None, None

    # Make wasi2ic executable
    if wasi2ic_tool.exists() and os.name != "nt":
        os.chmod(wasi2ic_tool, 0o755)

    return polyfill_lib, wasi2ic_tool


def setup_ic_tools(build_lib_dir: Path, use_release: bool = True) -> tuple:
    """
    Phase 2: Ensure IC-specific conversion tools are available.

    By default, attempts to download pre-built artifacts from GitHub release.
    Falls back to building from source if download fails or use_release=False.

    - libic_wasi_polyfill.a: WASI polyfill for IC environment
    - wasi2ic: WASI to IC WASM converter

    Args:
        build_lib_dir: Directory to place artifacts
        use_release: If True (default), try downloading from release first

    Returns:
        (polyfill_lib_path, wasi2ic_tool_path)
    """
    print("\n[Phase 2] IC Tools: polyfill + wasi2ic")

    # Try downloading from release first (default behavior)
    if use_release:
        print(" Attempting to download pre-built artifacts from release...")
        polyfill_lib, wasi2ic_tool = _try_download_ic_tools(
            build_lib_dir, use_release=True
        )
        if (
            polyfill_lib
            and polyfill_lib.exists()
            and wasi2ic_tool
            and wasi2ic_tool.exists()
        ):
            print(f" Polyfill: {polyfill_lib.name} (from release)")
            print(f" wasi2ic: {wasi2ic_tool.name} (from release)")
            return polyfill_lib, wasi2ic_tool
        print(" Download failed, falling back to building from source...")

    # Fallback to building from source
    print(" Building from source...")
    polyfill_lib = ensure_polyfill_library(build_lib_dir)
    if not polyfill_lib or not polyfill_lib.exists():
        print(" Error: Failed to build polyfill library.")
        sys.exit(1)
    print(f" Polyfill: {polyfill_lib.name}")

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


def build(wasi: bool = False, examples: Optional[List[str]] = None, run_tests: bool = False) -> None:
    """
    Main build orchestrator.

    WASI build phases:
      1. Toolchain    - WASI SDK
      2. IC Tools     - polyfill + wasi2ic
      3. Build        - CMake + compile
      4. Post-process - wasi2ic -> wasm-opt -> candid -> dfx.json

    Native build phases:
      3. Build        - CMake + compile
      (Optional) Tests - Run ctest if --test specified
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
        # Default: try downloading from release, fallback to building
        # Set use_release=False to force building from source
        polyfill_lib, wasi2ic_tool = setup_ic_tools(build_lib_dir, use_release=True)
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

    # Native: run tests only if requested
    if not wasi and run_tests:
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
  python build.py --help     Show this help message
  python build.py            Native build (without tests)
  python build.py --test     Native build and run tests
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

    parser.add_argument(
        "-t", "--test",
        action="store_true",
        dest="run_tests",
        help="Run tests after native build (ignored for WASM builds)",
    )

    args = parser.parse_args()

    # If no arguments provided, print help and exit
    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(0)

    if args.new:
        create_new_project(args.new, _ROOT_DIR)
        sys.exit(0)

    try:
        build(wasi=args.icwasm, examples=args.examples, run_tests=args.run_tests)
    except subprocess.CalledProcessError as e:
        print(f"\n✗ Build failed: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n✗ Build interrupted.")
        sys.exit(130)
