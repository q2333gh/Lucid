#!/usr/bin/env python3
import subprocess
import pathlib
import sys
import argparse
import os


def build(wasi=False):
    BUILD_DIR = pathlib.Path("build")
    BUILD_DIR.mkdir(exist_ok=True)

    cmake_args = ["cmake", ".."]

    if wasi:
        print(" Targeting WASI/WASM...")
        # Enable WASI build flag for CMake
        cmake_args.append("-DBUILD_WASI=ON")

        # Optional: If you have a WASI toolchain file, specify it here
        # cmake_args.append("-DCMAKE_TOOLCHAIN_FILE=path/to/wasi-toolchain.cmake")

        # Or if using WASI SDK via environment variables:
        if os.environ.get("WASI_SDK_ROOT"):
            print(f" Using WASI SDK at {os.environ.get('WASI_SDK_ROOT')}")
            # You might need to set CMAKE_C_COMPILER etc. here if not using a toolchain file
    else:
        print(" Targeting Native (Host)...")

    print(" Configuring CMake...")
    subprocess.run(cmake_args, cwd=BUILD_DIR, check=True)

    print(" Building project...")
    subprocess.run(["cmake", "--build", "."], cwd=BUILD_DIR, check=True)

    # Step 2: Run custom CLI (WASI or native)
    # Example of running the tool if built natively
    if not wasi:
        tool_path = (
            BUILD_DIR / "bin" / "c_candid_encode"
        )  # Adjust binary name as needed
        if tool_path.exists():
            print(f" Tool built at: {tool_path}")
            # subprocess.run([str(tool_path), "--help"], check=False)

    print(" Build complete.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build IC C SDK project")
    parser.add_argument(
        "--wasi", action="store_true", help="Target WASI platform (builds examples)"
    )
    args = parser.parse_args()

    try:
        build(wasi=args.wasi)
    except subprocess.CalledProcessError as e:
        print(f"Build failed with error: {e}")
        sys.exit(1)
