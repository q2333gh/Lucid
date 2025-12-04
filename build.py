#!/usr/bin/env python3
"""
IC C SDK Build Script

A streamlined build system for IC (Internet Computer) C SDK projects.
Supports both native and WASI/WASM targets.

Usage:
    python build.py           # Build for native platform
    python build.py --wasi    # Build for WASI/IC platform
"""

import argparse
import multiprocessing
import shutil
import subprocess
import sys
from pathlib import Path

# =============================================================================
# Module Loading
# =============================================================================

_ROOT_DIR = Path(__file__).parent.resolve()
_BUILD_UTILS_DIR = _ROOT_DIR / "cdk-c/scripts/build_utils"

# Add build_utils to Python path for imports
if str(_BUILD_UTILS_DIR) not in sys.path:
    sys.path.insert(0, str(_BUILD_UTILS_DIR))

# Import build utilities as functions
from wasi_sdk import ensure_wasi_sdk
from builders import ensure_polyfill_library, ensure_wasi2ic_tool
from post_process import post_process_wasm_files


# =============================================================================
# Build Configuration
# =============================================================================


def get_build_dirs(wasi: bool) -> dict:
    """Get build directory paths based on target platform."""
    return {
        "root": _ROOT_DIR,
        "build": _ROOT_DIR / ("build-wasi" if wasi else "build"),
        "build_lib": _ROOT_DIR / "build_lib",
        "examples": _ROOT_DIR / "examples",
    }


# =============================================================================
# CMake Build
# =============================================================================


def configure_cmake(build_dir: Path, wasi: bool, extra_args: list) -> None:
    """Configure CMake with appropriate arguments."""
    use_ninja = shutil.which("ninja") is not None

    if use_ninja:
        print(" Using Ninja build system")
    else:
        print(" Warning: Ninja not found, using default generator")

    cmake_args = ["cmake", "..", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]

    if use_ninja:
        cmake_args.extend(["-G", "Ninja"])

    if wasi:
        cmake_args.extend(["-DBUILD_WASI=ON", "-DCMAKE_BUILD_TYPE=MinSizeRel"])
    else:
        cmake_args.extend(["-DBUILD_CDK_TESTS=ON", "-DC_CANDID_BUILD_TESTS=ON"])

    cmake_args.extend(extra_args)

    print("\n[Configuring CMake]")
    subprocess.run(cmake_args, cwd=build_dir, check=True)


def run_build(build_dir: Path) -> None:
    """Execute the build."""
    print("\n[Building Project]")

    if shutil.which("ninja"):
        jobs = multiprocessing.cpu_count()
        subprocess.run(["ninja", "-j", str(jobs)], cwd=build_dir, check=True)
    else:
        subprocess.run(["cmake", "--build", "."], cwd=build_dir, check=True)


def run_tests(build_dir: Path) -> None:
    """Run tests (native build only)."""
    print("\n[Running Tests]")
    subprocess.run(["ctest", "--output-on-failure"], cwd=build_dir, check=False)


def copy_compile_commands(build_dir: Path) -> None:
    """Copy compile_commands.json to project root for IDE support."""
    src = build_dir / "compile_commands.json"
    if src.exists():
        shutil.copy(src, _ROOT_DIR / "compile_commands.json")
        print(f" Copied compile_commands.json to project root")


# =============================================================================
# Main Build Function
# =============================================================================


def build(wasi: bool = False) -> None:
    """
    Main build function.

    Args:
        wasi: If True, build for WASI/IC platform; otherwise build native.
    """
    dirs = get_build_dirs(wasi)
    dirs["build"].mkdir(exist_ok=True)
    dirs["build_lib"].mkdir(exist_ok=True)

    cmake_extra_args = []
    wasi2ic_tool = None

    # -------------------------------------------------------------------------
    # Platform Setup
    # -------------------------------------------------------------------------
    if wasi:
        print(" Targeting WASI/WASM...")

        # 1. WASI SDK
        sdk_path, toolchain_file = ensure_wasi_sdk()
        if not sdk_path or not toolchain_file:
            print(" Error: WASI SDK not found. Set WASI_SDK_ROOT environment variable.")
            sys.exit(1)

        print(f" WASI SDK: {sdk_path}")
        cmake_extra_args.extend(
            [
                f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
                f"-DWASI_SDK_PREFIX={sdk_path}",
            ]
        )

        # 2. Polyfill Library
        polyfill_lib = ensure_polyfill_library(dirs["build_lib"])
        if polyfill_lib and polyfill_lib.exists():
            cmake_extra_args.append(f"-DIC_WASI_POLYFILL_PATH={polyfill_lib}")
        else:
            print(" Error: Failed to build polyfill library.")
            sys.exit(1)

        # 3. wasi2ic Tool
        wasi2ic_tool = ensure_wasi2ic_tool(dirs["build_lib"])
        if not wasi2ic_tool or not wasi2ic_tool.exists():
            print(
                " Warning: wasi2ic tool not available, post-processing will be skipped."
            )

    else:
        print(" Targeting Native (Host)...")

    # -------------------------------------------------------------------------
    # Build
    # -------------------------------------------------------------------------
    print(f"\n[Configuration]")
    print(f" Build Directory: {dirs['build']}")

    configure_cmake(dirs["build"], wasi, cmake_extra_args)
    run_build(dirs["build"])

    if not wasi:
        run_tests(dirs["build"])

    copy_compile_commands(dirs["build"])

    # -------------------------------------------------------------------------
    # Post-processing (WASI only)
    # -------------------------------------------------------------------------
    if wasi and wasi2ic_tool and wasi2ic_tool.exists():
        print("\n[Post-processing WASM files]")
        bin_dir = dirs["build"] / "bin"

        if bin_dir.exists():
            stats = post_process_wasm_files(
                bin_dir=bin_dir,
                wasi2ic_tool=wasi2ic_tool,
                examples_dir=dirs["examples"],
            )
            print(
                f"\n Summary: {stats['converted']} converted, "
                f"{stats['optimized']} optimized, "
                f"{stats['candid']} .did extracted"
            )

    print("\nBuild complete.")


# =============================================================================
# Entry Point
# =============================================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Build IC C SDK project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python build.py           Build for native platform (with tests)
  python build.py --wasi    Build for WASI/IC platform
        """,
    )
    parser.add_argument(
        "--wasi",
        action="store_true",
        help="Target WASI platform (builds examples for IC)",
    )

    args = parser.parse_args()

    try:
        build(wasi=args.wasi)
    except subprocess.CalledProcessError as e:
        print(f"\nBuild failed: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nBuild interrupted.")
        sys.exit(130)
