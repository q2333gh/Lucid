#!/usr/bin/env python3
"""
Configuration module for IC C SDK build system
Manages paths, compiler settings, and build options
"""

import os
import platform
from pathlib import Path


########################################################################
# System Information
########################################################################

OS_SYSTEM = platform.system()
OS_PROCESSOR = platform.processor()


########################################################################
# Path Configuration
########################################################################

def find_project_root(start_path: Path) -> Path:
    """
    Find project root directory by looking for characteristic directories.
    
    The project root is identified by the presence of 'src' and 'include' directories.
    This allows the script to work whether it's in the project root or scripts/ subdirectory.
    """
    current = start_path.resolve()
    
    # Check if we're in scripts/ directory, project root is parent
    if current.name == "scripts" and (current.parent / "src").exists() and (current.parent / "include").exists():
        return current.parent
    
    # Otherwise, search upward for project root
    for parent in [current] + list(current.parents):
        if (parent / "src").exists() and (parent / "include").exists():
            return parent
    
    # Fallback: if script is in scripts/, use parent; otherwise use current
    if current.name == "scripts":
        return current.parent
    
    return current


def initialize_paths(script_dir: Path):
    """Initialize all project paths based on script directory"""
    project_root = find_project_root(script_dir)
    
    return {
        'PROJECT_ROOT': project_root,
        'SRC_DIR': project_root / "src",
        'INCLUDE_DIR': project_root / "include",
        'EXAMPLES_DIR': project_root / "examples/hello_lucid",
        'SCRIPTS_DIR': project_root / "scripts",
        'BUILD_DIR': project_root / "build",
        'WASI_BUILD_DIR': project_root / "build",
    }


########################################################################
# WASI SDK Configuration
########################################################################

# Locked versions from dependency_commit_report.md
IC_WASI_POLYFILL_COMMIT = "b3ef005140e7eebf7d0b5471ccc3a6d4cbec4ee5"
WASI2IC_COMMIT = "c0f5063e734f8365f1946baf2845d8322cc9bfec"

WASI_SDK_VERSION = os.environ.get("WASI_SDK_VERSION", "25.0")


def get_wasi_sdk_paths():
    """Get WASI SDK paths from environment or default"""
    wasi_sdk_root = Path(os.environ.get("WASI_SDK_ROOT", "ERROR"))
    wasi_sdk_compiler_root = wasi_sdk_root / f"wasi-sdk-{WASI_SDK_VERSION}"
    
    return {
        'WASI_SDK_ROOT': wasi_sdk_root,
        'WASI_SDK_COMPILER_ROOT': wasi_sdk_compiler_root,
        'WASI_C': wasi_sdk_compiler_root / "bin/clang",
        'WASI_AR': wasi_sdk_compiler_root / "bin/llvm-ar",
        'WASI_SYSROOT': wasi_sdk_compiler_root / "share/wasi-sysroot",
    }


########################################################################
# Native Compiler Configuration
########################################################################

NATIVE_C = "clang"
NATIVE_AR = "ar"


########################################################################
# Compilation Options
########################################################################

# Basic compilation options
CFLAGS_WALL = "-Wall"
CFLAGS_WEXTRA = "-Wextra"
CFLAGS_STD = "-std=c11"
CFLAGS_DEBUG = "-g"
CFLAGS_OPTIMIZE = "-O3"


def get_compile_flags(include_dir: Path, target_platform: str = "native"):
    """Get compilation flags for the specified platform"""
    cflags_include = f"-I{include_dir}"
    
    if target_platform == "wasi":
        wasi_paths = get_wasi_sdk_paths()
        return {
            'CFLAGS': [
                CFLAGS_WALL, CFLAGS_WEXTRA, CFLAGS_STD, CFLAGS_OPTIMIZE,
                "--target=wasm32-wasi",
                f"--sysroot={wasi_paths['WASI_SYSROOT']}",
                "-flto",
                "-fvisibility=hidden",
                "-DNDEBUG",
                cflags_include
            ],
            'LDFLAGS': [
                "-mexec-model=reactor",
                "-Wl,--lto-O3",
                "-Wl,--strip-debug",
                "-Wl,--stack-first",
                "-Wl,--export-dynamic"
            ]
        }
    else:  # native
        return {
            'CFLAGS': [
                CFLAGS_WALL, CFLAGS_WEXTRA, CFLAGS_STD, CFLAGS_DEBUG, cflags_include
            ],
            'LDFLAGS': []
        }


########################################################################
# Source File Configuration
########################################################################

# Library source files
LIB_SOURCES = [
    "ic_api.c",
    "ic_buffer.c",
    "ic_candid.c",
    "ic_principal.c",
    "ic_wasi_polyfill.c"
]

# Library name
LIB_NAME = "ic_c_sdk"


def get_library_name(target_platform: str = "native"):
    """Get library name for the specified platform"""
    return f"lib{LIB_NAME}.a"

