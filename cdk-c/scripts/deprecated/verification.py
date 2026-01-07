#!/usr/bin/env python3
"""
WASM verification functions for IC C SDK build system
"""

from pathlib import Path
from typing import Optional, List, Tuple

import sh
from rich.console import Console

# Removed relative import that causes issues when loaded via importlib
# from .config import get_wasi_sdk_paths

console = Console(force_terminal=True, markup=True)
print = console.print


def _resolve_wasm_objdump(wasi_sdk_compiler_root: Optional[Path]):
    """
    Prefer wasm-objdump from WASI SDK; fall back to PATH.
    Returns sh.Command or None if unavailable.
    """
    if wasi_sdk_compiler_root:
        wasm_objdump_path = wasi_sdk_compiler_root / "bin" / "wasm-objdump"
        if wasm_objdump_path.exists():
            return sh.Command(str(wasm_objdump_path))

    try:
        return sh.wasm_objdump
    except AttributeError:
        return None


def _parse_raw_init_import(lines: List[str]) -> Tuple[bool, bool, str]:
    """
    Scan disassembly lines for raw_init import.
    Returns (import_found, polyfill_import_found, import_line).
    """
    import_found = False
    import_line = ""
    polyfill_import_found = False

    for i, line in enumerate(lines):
        line_lower = line.lower()
        if "raw_init" not in line_lower or "func[" not in line_lower:
            continue

        import_found = True
        import_line = line.strip()
        if "polyfill" in line_lower or "<- polyfill" in line_lower:
            polyfill_import_found = True

        # Look around for module hints
        for j in range(max(0, i - 2), min(len(lines), i + 3)):
            neighbor = lines[j].lower()
            if "polyfill" in neighbor and "<-" in neighbor:
                polyfill_import_found = True
                import_line += " / " + lines[j].strip()
                break
        break

    return import_found, polyfill_import_found, import_line


def verify_raw_init_import(
    wasm_file: Path, wasi_sdk_compiler_root: Optional[Path] = None
) -> bool:
    """
    Verify that raw_init import is present in the WASM file

    Args:
        wasm_file: Path to WASM file to verify
        wasi_sdk_compiler_root: Path to WASI SDK compiler root (optional)

    Returns:
        True if verification passed or tool unavailable, False if verification failed
    """
    wasm_objdump_cmd = _resolve_wasm_objdump(wasi_sdk_compiler_root)

    if not wasm_objdump_cmd:
        error_msg = "wasm-objdump not found"
        print(
            f"[yellow]  Could not verify raw_init import (wasm-objdump not available: {error_msg})[/]"
        )
        print(
            f"     Manually verify that {wasm_file.name} contains 'polyfill::raw_init' import"
        )
        print(f"     You can use: wasm-objdump -x {wasm_file}")
        return True

    try:
        output = (
            wasm_objdump_cmd("-x", str(wasm_file), _iter=False)
            if wasm_objdump_cmd
            else ""
        )
    except sh.ErrorReturnCode as e:
        error_msg = str(e)
        print(
            f"[yellow]  Could not verify raw_init import (wasm-objdump failed: {error_msg})[/]"
        )
        print(
            f"     Manually verify that {wasm_file.name} contains 'polyfill::raw_init' import"
        )
        print(f"     You can use: wasm-objdump -x {wasm_file}")
        return True
    except AttributeError:
        error_msg = "wasm-objdump not found"
        output = ""

    if not output:
        error_msg = "no output from wasm-objdump"
        print(
            f"[yellow]  Could not verify raw_init import (wasm-objdump not available: {error_msg})[/]"
        )
        print(
            f"     Manually verify that {wasm_file.name} contains 'polyfill::raw_init' import"
        )
        print(f"     You can use: wasm-objdump -x {wasm_file}")
        return True  # Don't fail if verification tool is not available

    lines = output.split("\n")
    import_found, polyfill_import_found, import_line = _parse_raw_init_import(lines)

    if import_found:
        if polyfill_import_found:
            print(
                f"[green]  Verified: raw_init import from polyfill module found in {wasm_file.name}[/]"
            )
        else:
            print(
                f"[green]  Verified: raw_init import found in {wasm_file.name} (may be converted by --import-undefined)[/]"
            )
        print(f"     Import: {import_line}")
        return True
    else:
        print(f"[red]  Error: raw_init import NOT found in {wasm_file.name}[/]")
        print("     Expected: import from 'polyfill' module, function 'raw_init'")
        print("     This indicates LTO optimization removed the unused import")
        print(
            "     Note: raw_init is declared as import in source code, but LTO removed it"
        )
        print("     Possible solutions:")
        print(
            "       1. Ensure __ic_wasi_polyfill_start is called (it's exported as 'start')"
        )
        print("       2. Check if --import-undefined linker option is supported")
        print("       3. Consider disabling LTO for this module")
        print("     Available imports in WASM file:")
        import_count = 0
        for line in lines:
            if "import[" in line.lower() or ("<-" in line and "func[" in line.lower()):
                if import_count < 10:  # Show first 10 imports
                    print(f"       {line.strip()}")
                    import_count += 1
                elif import_count == 10:
                    print(f"       ... (showing first 10 imports)")
                    break
        return False
