#!/usr/bin/env python3
"""
Build utilities package for IC C SDK build system
"""

from .config import (
    find_project_root,
    initialize_paths,
    get_wasi_sdk_paths,
    get_compile_flags,
    LIB_SOURCES,
    LIB_NAME,
    get_library_name,
    IC_WASI_POLYFILL_COMMIT,
    WASI2IC_COMMIT,
    NATIVE_C,
    NATIVE_AR,
    WASI_SDK_VERSION,
)

from .utils import (
    run_command,
    find_polyfill_library,
    find_wasi2ic_tool,
    ensure_polyfill_library,
    ensure_wasi2ic_tool,
    check_source_files_exist,
    needs_rebuild,
    find_wasm_opt,
    optimize_wasm,
)

from .verification import verify_raw_init_import

__all__ = [
    'find_project_root',
    'initialize_paths',
    'get_wasi_sdk_paths',
    'get_compile_flags',
    'LIB_SOURCES',
    'LIB_NAME',
    'get_library_name',
    'IC_WASI_POLYFILL_COMMIT',
    'WASI2IC_COMMIT',
    'NATIVE_C',
    'NATIVE_AR',
    'WASI_SDK_VERSION',
    'run_command',
    'find_polyfill_library',
    'find_wasi2ic_tool',
    'ensure_polyfill_library',
    'ensure_wasi2ic_tool',
    'check_source_files_exist',
    'needs_rebuild',
    'find_wasm_opt',
    'optimize_wasm',
    'verify_raw_init_import',
]

