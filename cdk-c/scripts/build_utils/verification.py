#!/usr/bin/env python3
"""
WASM verification functions for IC C SDK build system
"""

from pathlib import Path
from typing import Optional

import sh

from .config import get_wasi_sdk_paths


def verify_raw_init_import(wasm_file: Path, wasi_sdk_compiler_root: Optional[Path] = None) -> bool:
    """
    Verify that raw_init import is present in the WASM file
    
    Args:
        wasm_file: Path to WASM file to verify
        wasi_sdk_compiler_root: Path to WASI SDK compiler root (optional)
    
    Returns:
        True if verification passed or tool unavailable, False if verification failed
    """
    # Try to use wasm-objdump (from WASI SDK) to check imports
    if wasi_sdk_compiler_root:
        wasm_objdump_path = wasi_sdk_compiler_root / "bin" / "wasm-objdump"
        if not wasm_objdump_path.exists():
            # Fallback: try to find wasm-objdump in PATH
            wasm_objdump_cmd = sh.wasm_objdump
        else:
            wasm_objdump_cmd = sh.Command(str(wasm_objdump_path))
    else:
        # Fallback: try to find wasm-objdump in PATH
        wasm_objdump_cmd = sh.wasm_objdump
    
    try:
        # Use wasm-objdump to list imports
        output = wasm_objdump_cmd("-x", str(wasm_file), _iter=False) or ""
    except (sh.ErrorReturnCode, AttributeError) as e:
        # If wasm-objdump is not available, warn but don't fail
        error_msg = str(e)
        print(f"  ⚠️  Could not verify raw_init import (wasm-objdump not available: {error_msg})")
        print(f"     Manually verify that {wasm_file.name} contains 'polyfill::raw_init' import")
        print(f"     You can use: wasm-objdump -x {wasm_file}")
        return True  # Don't fail if verification tool is not available
    
    # Check if raw_init import exists (may be from polyfill module or as undefined import)
    import_found = False
    import_line = ""
    polyfill_import_found = False
    
    # Search for raw_init function in imports
    lines = output.split('\n')
    for i, line in enumerate(lines):
        line_lower = line.lower()
        # Check if line contains raw_init and is in Import section
        # Format can be: " - func[X] sig=Y <raw_init> <- polyfill.raw_init"
        # Or just: " - func[X] sig=Y <raw_init>" (if converted by --import-undefined)
        if 'raw_init' in line_lower and 'func[' in line_lower:
            import_found = True
            import_line = line.strip()
            # Check if it's from polyfill module (look for <- polyfill)
            if 'polyfill' in line_lower or '<- polyfill' in line_lower:
                polyfill_import_found = True
            # Also check surrounding lines for module info
            for j in range(max(0, i-2), min(len(lines), i+3)):
                if 'polyfill' in lines[j].lower() and '<-' in lines[j].lower():
                    polyfill_import_found = True
                    import_line += " / " + lines[j].strip()
                    break
            break
    
    if import_found:
        if polyfill_import_found:
            print(f"  ✅ Verified: raw_init import from polyfill module found in {wasm_file.name}")
        else:
            print(f"  ✅ Verified: raw_init import found in {wasm_file.name} (may be converted by --import-undefined)")
        print(f"     Import: {import_line}")
        return True
    else:
        print(f"  ❌ Error: raw_init import NOT found in {wasm_file.name}")
        print(f"     Expected: import from 'polyfill' module, function 'raw_init'")
        print(f"     This indicates LTO optimization removed the unused import")
        print(f"     Note: raw_init is declared as import in source code, but LTO removed it")
        print(f"     Possible solutions:")
        print(f"       1. Ensure __ic_wasi_polyfill_start is called (it's exported as 'start')")
        print(f"       2. Check if --import-undefined linker option is supported")
        print(f"       3. Consider disabling LTO for this module")
        # Print all imports for debugging
        print(f"     Available imports in WASM file:")
        import_count = 0
        for line in lines:
            if 'import[' in line.lower() or ('<-' in line and 'func[' in line.lower()):
                if import_count < 10:  # Show first 10 imports
                    print(f"       {line.strip()}")
                    import_count += 1
                elif import_count == 10:
                    print(f"       ... (showing first 10 imports)")
                    break
        return False
