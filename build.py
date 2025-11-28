#!/usr/bin/env python3
import subprocess
import pathlib
import sys
import argparse
import os
import shutil


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


def build(wasi=False):
    ROOT_DIR = pathlib.Path(__file__).parent.resolve()
    BUILD_DIR = ROOT_DIR / "build"
    BUILD_LIB_DIR = ROOT_DIR / "build_lib"

    BUILD_DIR.mkdir(exist_ok=True)
    BUILD_LIB_DIR.mkdir(exist_ok=True)

    cmake_args = ["cmake", "..", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]

    # Store paths for post-processing
    wasi2ic_tool = None
    generated_wasm_files = []

    if wasi:
        print(" Targeting WASI/WASM...")
        cmake_args.append("-DBUILD_WASI=ON")

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

    # Step 2: Post-processing (WASI only)
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
