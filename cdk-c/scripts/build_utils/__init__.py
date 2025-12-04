#!/usr/bin/env python3
"""
Build utilities package for IC C SDK build system.

Modules:
- config: Path and compiler configuration
- utils: General build utilities
- wasi_sdk: WASI SDK discovery
- wasm_opt: WASM optimization
- extract_candid: Candid interface extraction
- post_process: Post-build processing pipeline
- verification: WASM artifact verification
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

from .wasi_sdk import (
    find_wasi_sdk_root,
    find_toolchain_file,
    ensure_wasi_sdk,
)

from .wasm_opt import (
    find_wasm_opt as find_wasm_opt_tool,
    optimize_wasm as optimize_wasm_file,
)

from .extract_candid import (
    find_candid_extractor,
    extract_candid,
    extract_candid_for_examples,
)

from .post_process import (
    convert_wasi_to_ic,
    post_process_wasm_files,
)

from .builders import (
    check_rust_toolchain,
    build_polyfill_library,
    build_wasi2ic_tool,
    ensure_polyfill_library as ensure_polyfill,
    ensure_wasi2ic_tool as ensure_wasi2ic,
)

from .verification import verify_raw_init_import

__all__ = [
    # config
    "find_project_root",
    "initialize_paths",
    "get_wasi_sdk_paths",
    "get_compile_flags",
    "LIB_SOURCES",
    "LIB_NAME",
    "get_library_name",
    "IC_WASI_POLYFILL_COMMIT",
    "WASI2IC_COMMIT",
    "NATIVE_C",
    "NATIVE_AR",
    "WASI_SDK_VERSION",
    # utils
    "run_command",
    "find_polyfill_library",
    "find_wasi2ic_tool",
    "ensure_polyfill_library",
    "ensure_wasi2ic_tool",
    "check_source_files_exist",
    "needs_rebuild",
    "find_wasm_opt",
    "optimize_wasm",
    # wasi_sdk
    "find_wasi_sdk_root",
    "find_toolchain_file",
    "ensure_wasi_sdk",
    # wasm_opt
    "find_wasm_opt_tool",
    "optimize_wasm_file",
    # extract_candid
    "find_candid_extractor",
    "extract_candid",
    "extract_candid_for_examples",
    # post_process
    "convert_wasi_to_ic",
    "post_process_wasm_files",
    # builders
    "check_rust_toolchain",
    "build_polyfill_library",
    "build_wasi2ic_tool",
    "ensure_polyfill",
    "ensure_wasi2ic",
    # verification
    "verify_raw_init_import",
]
