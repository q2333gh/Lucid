#!/usr/bin/env python3
import subprocess
import pathlib
import sys
import argparse
import os
import shutil
import importlib.util


def check_command(cmd):
    return shutil.which(cmd) is not None


def ensure_wasi_sdk():
    """
    Finds the WASI SDK path and the CMake toolchain file.
    Returns (sdk_root_path, toolchain_file_path) or (None, None) if not found.
    """
    # 1. Check environment variable
    wasi_sdk_root = os.environ.get("WASI_SDK_ROOT")
    if not wasi_sdk_root:
        # 2. Check common locations
        common_paths = [
            "/opt/wasi-sdk",
            "/usr/local/opt/wasi-sdk",
            str(pathlib.Path.home() / "opt/wasi-sdk"),
            str(pathlib.Path.home() / "wasi-sdk"),
        ]
        for p in common_paths:
            if pathlib.Path(p).exists():
                wasi_sdk_root = p
                break

    if not wasi_sdk_root:
        return None, None

    wasi_sdk_path = pathlib.Path(wasi_sdk_root)

    # Look for the toolchain file
    # It's usually in share/cmake/wasi-sdk.cmake or share/cmake/wasi-sdk-xx.x/wasi-sdk.cmake
    toolchain_file = wasi_sdk_path / "share/cmake/wasi-sdk.cmake"

    if not toolchain_file.exists():
        # Try globbing for versioned directory
        found = list(wasi_sdk_path.glob("share/cmake/wasi-sdk-*/wasi-sdk.cmake"))
        if found:
            toolchain_file = found[0]

    if not toolchain_file.exists():
        print(
            f" Warning: Found WASI SDK at {wasi_sdk_path}, but could not find 'share/cmake/wasi-sdk.cmake'."
        )
        return None, None

    return wasi_sdk_path, toolchain_file


def find_wasm_opt():
    """Find wasm-opt tool in PATH"""
    wasm_opt = shutil.which("wasm-opt")
    return pathlib.Path(wasm_opt) if wasm_opt else None


def optimize_wasm(
    input_file: pathlib.Path, output_file: pathlib.Path, optimization_level: str = "-Oz"
) -> bool:
    """
    Optimize WASM file using wasm-opt.

    Args:
        input_file: Path to input WASM file
        output_file: Path to output optimized WASM file
        optimization_level: Optimization level (-Oz for size, -O4 for speed)

    Returns:
        True if optimization succeeded, False otherwise
    """
    wasm_opt = find_wasm_opt()
    if not wasm_opt:
        print(" Warning: wasm-opt not found in PATH, skipping optimization")
        print(
            "   Install binaryen: sudo apt install binaryen (or brew install binaryen)"
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
                f" Optimized: {original_size:,} -> {optimized_size:,} bytes ({reduction:.1f}% reduction)"
            )
            return True
        else:
            print(f" Warning: wasm-opt completed but output file not found")
            return False

    except subprocess.CalledProcessError as e:
        print(
            f" Warning: wasm-opt failed: {e.stderr.decode() if e.stderr else 'unknown error'}"
        )
        return False


def verify_wasi_artifact(wasm_file: pathlib.Path, sdk_path: pathlib.Path) -> bool:
    """Dynamically load verification module and verify WASM artifact"""
    script_path = pathlib.Path("cdk-c/scripts/build_utils/verification.py").resolve()
    if not script_path.exists():
        print(f" Warning: Verification script not found at {script_path}")
        return True

    try:
        spec = importlib.util.spec_from_file_location("verification", script_path)
        if spec and spec.loader:
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            if hasattr(module, "verify_raw_init_import"):
                # Note: verify_raw_init_import expects compiler_root, which is what sdk_path should be
                return module.verify_raw_init_import(wasm_file, sdk_path)
    except Exception as e:
        print(f" Warning: Failed to run verification: {e}")

    return True


def build(wasi=False):
    ROOT_DIR = pathlib.Path(__file__).parent.resolve()

    # Use separate build directories for Native and WASI to avoid cache pollution
    if wasi:
        BUILD_DIR = ROOT_DIR / "build-wasi"
    else:
        BUILD_DIR = ROOT_DIR / "build"

    BUILD_LIB_DIR = ROOT_DIR / "build_lib"

    BUILD_DIR.mkdir(exist_ok=True)
    BUILD_LIB_DIR.mkdir(exist_ok=True)

    cmake_args = ["cmake", "..", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]

    if not wasi:
        # Enable tests for native build
        cmake_args.append("-DBUILD_CDK_TESTS=ON")
        cmake_args.append("-DC_CANDID_BUILD_TESTS=ON")

    # Store paths for post-processing
    wasi2ic_tool = None
    sdk_path = None  # Store SDK path for verification

    if wasi:
        print(" Targeting WASI/WASM...")
        cmake_args.append("-DBUILD_WASI=ON")
        cmake_args.append("-DCMAKE_BUILD_TYPE=MinSizeRel")

        # 1. Setup WASI SDK
        sdk_path, toolchain_file = ensure_wasi_sdk()
        if sdk_path and toolchain_file:
            print(f" Using WASI SDK: {sdk_path}")
            print(f" Using Toolchain: {toolchain_file}")
            cmake_args.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}")
            cmake_args.append(f"-DWASI_SDK_PREFIX={sdk_path}")
        else:
            print(" Error: WASI SDK not found. Please set WASI_SDK_ROOT.")
            sys.exit(1)

        # 2. Ensure Polyfill Library (libic_wasi_polyfill.a)
        polyfill_lib = BUILD_LIB_DIR / "libic_wasi_polyfill.a"
        if not polyfill_lib.exists():
            print(" Building ic-wasi-polyfill library...")
            polyfill_script = (
                ROOT_DIR / "cdk-c/scripts/build_utils/build_libic_wasi_polyfill.py"
            )
            if polyfill_script.exists():
                try:
                    subprocess.run(
                        [
                            sys.executable,
                            str(polyfill_script),
                            "--output-dir",
                            str(BUILD_LIB_DIR),
                        ],
                        check=True,
                    )
                except subprocess.CalledProcessError:
                    print(" Failed to build polyfill library.")
                    sys.exit(1)
            else:
                print(f" Error: Polyfill build script missing at {polyfill_script}")
                sys.exit(1)

        if polyfill_lib.exists():
            cmake_args.append(f"-DIC_WASI_POLYFILL_PATH={polyfill_lib}")

        # 3. Ensure wasi2ic tool
        wasi2ic_tool = BUILD_LIB_DIR / "wasi2ic"
        if not wasi2ic_tool.exists():
            print(" Building wasi2ic tool...")
            wasi2ic_script = ROOT_DIR / "cdk-c/scripts/build_utils/build_wasi2ic.py"
            if wasi2ic_script.exists():
                try:
                    subprocess.run(
                        [
                            sys.executable,
                            str(wasi2ic_script),
                            "--output-dir",
                            str(BUILD_LIB_DIR),
                        ],
                        check=True,
                    )
                except subprocess.CalledProcessError:
                    print(" Failed to build wasi2ic tool.")
                    # Can continue, but post-processing will fail
            else:
                print(f" Warning: wasi2ic script missing at {wasi2ic_script}")

    else:
        print(" Targeting Native (Host)...")

    print("\n[Configuration]")
    print(f" Build Directory: {BUILD_DIR}")
    print(f" CMake Args: {' '.join(cmake_args)}")

    print("\n[Configuring CMake]")
    subprocess.run(cmake_args, cwd=BUILD_DIR, check=True)

    print("\n[Building Project]")
    subprocess.run(["cmake", "--build", "."], cwd=BUILD_DIR, check=True)

    if not wasi:
        print("\n[Running Tests]")
        try:
            subprocess.run(["ctest", "--output-on-failure"], cwd=BUILD_DIR, check=False)
        except Exception:
            print(" Tests failed to run.")

    # Copy compile_commands.json to project root for IDE support
    compile_commands = BUILD_DIR / "compile_commands.json"
    if compile_commands.exists():
        try:
            shutil.copy(compile_commands, ROOT_DIR / "compile_commands.json")
            print(f" Copied compile_commands.json to {ROOT_DIR}")
        except Exception as e:
            print(f" Warning: Failed to copy compile_commands.json: {e}")

    # Step 2: Post-processing (WASI only)
    # wasi2ic + wasm-opt
    if wasi and wasi2ic_tool and wasi2ic_tool.exists():
        print("\n[Post-processing WASM files]")
        # Find generated .wasm files in build/bin (default CMake runtime output)
        bin_dir = BUILD_DIR / "bin"
        if bin_dir.exists():
            for wasm_file in bin_dir.glob("*.wasm"):
                if "_ic.wasm" in wasm_file.name:
                    continue  # Skip already converted

                output_wasm = bin_dir / f"{wasm_file.stem}_ic.wasm"
                print(f" Converting {wasm_file.name} -> {output_wasm.name}...")
                try:
                    subprocess.run(
                        [str(wasi2ic_tool), str(wasm_file), str(output_wasm)],
                        check=True,
                    )
                    print(f" Success: {output_wasm}")

                    # Verify the output artifact              
                    # Optimize the IC-converted WASM file in-place
                    print(f" Optimizing {output_wasm.name}...")
                    temp_optimized = bin_dir / f"{output_wasm.stem}_opt_temp.wasm"
                    if optimize_wasm(output_wasm, temp_optimized, "-Oz"):
                        # Replace original with optimized version
                        shutil.move(str(temp_optimized), str(output_wasm))
                        print(f" Optimized file saved as: {output_wasm}")
                    else:
                        # Clean up temp file if it exists
                        if temp_optimized.exists():
                            temp_optimized.unlink()
                        print(f" Using unoptimized file: {output_wasm}")

                except subprocess.CalledProcessError:
                    print(f" Failed to convert {wasm_file.name}")

    print("\nBuild complete.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build IC C SDK project")
    parser.add_argument(
        "--wasi",
        action="store_true",
        help="Target WASI platform (builds examples for IC)",
    )
    args = parser.parse_args()

    try:
        build(wasi=args.wasi)
    except subprocess.CalledProcessError as e:
        print(f"Build failed with error: {e}")
        sys.exit(1)
