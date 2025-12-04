#!/usr/bin/env python3
"""
Candid interface extraction utility for IC WASM canisters.

Uses candid-extractor to generate .did files from compiled _ic.wasm files.
"""

import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional

# Handle imports for both direct execution and dynamic loading
_script_dir = Path(__file__).parent.resolve()
if str(_script_dir) not in sys.path:
    sys.path.insert(0, str(_script_dir))

try:
    from utils import console, command_exists
except ImportError:
    # Fallback: create minimal implementations if utils not available
    class _FallbackConsole:
        def print(self, msg, **kwargs):
            # Strip rich markup for plain print
            import re

            clean = re.sub(r"\[/?[^\]]+\]", "", str(msg))
            print(clean)

    console = _FallbackConsole()

    def command_exists(cmd: str) -> bool:
        return shutil.which(cmd) is not None


def find_candid_extractor() -> Optional[Path]:
    """Find candid-extractor tool in PATH."""
    extractor = shutil.which("candid-extractor")
    return Path(extractor) if extractor else None


def extract_candid(
    wasm_file: Path,
    output_did: Path,
) -> bool:
    """
    Extract Candid interface from a WASM file.

    Args:
        wasm_file: Path to the _ic.wasm file
        output_did: Path to output .did file

    Returns:
        True if extraction succeeded, False otherwise
    """
    extractor = find_candid_extractor()
    if not extractor:
        console.print(
            "[yellow]  candid-extractor not found in PATH, "
            "skipping .did generation[/]"
        )
        console.print("     Install: cargo install candid-extractor")
        return False

    if not wasm_file.exists():
        console.print(f"[red]  WASM file not found: {wasm_file}[/]")
        return False

    try:
        # Run candid-extractor and capture output
        result = subprocess.run(
            [str(extractor), str(wasm_file)],
            capture_output=True,
            text=True,
            check=True,
        )

        # Write output to .did file
        output_did.parent.mkdir(parents=True, exist_ok=True)
        output_did.write_text(result.stdout)

        console.print(
            f"[green]  Generated: {output_did.relative_to(output_did.parent.parent.parent)}[/]"
        )
        return True

    except subprocess.CalledProcessError as e:
        console.print(f"[red]  candid-extractor failed: {e.stderr}[/]")
        return False
    except Exception as e:
        console.print(f"[red]  Error extracting candid: {e}[/]")
        return False


def extract_candid_for_examples(
    bin_dir: Path,
    examples_dir: Path,
) -> int:
    """
    Extract Candid interfaces for all _ic.wasm files in bin_dir.

    Maps wasm files to their corresponding example directories:
      bin_dir/{name}_ic.wasm -> examples_dir/{name}/{name}.did

    Args:
        bin_dir: Directory containing _ic.wasm files
        examples_dir: Root examples directory

    Returns:
        Number of successfully extracted .did files
    """
    if not bin_dir.exists():
        console.print(f"[yellow]  Bin directory not found: {bin_dir}[/]")
        return 0

    extractor = find_candid_extractor()
    if not extractor:
        console.print(
            "[yellow]  candid-extractor not found, skipping .did generation[/]"
        )
        console.print("     Install: cargo install candid-extractor")
        return 0

    success_count = 0

    for wasm_file in bin_dir.glob("*_ic.wasm"):
        # Extract example name: hello_lucid_ic.wasm -> hello_lucid
        example_name = wasm_file.stem.replace("_ic", "")

        # Find corresponding example directory
        example_dir = examples_dir / example_name
        if not example_dir.exists():
            console.print(
                f"[yellow]  Example directory not found for {wasm_file.name}: "
                f"{example_dir}[/]"
            )
            continue

        # Output .did file with example name
        output_did = example_dir / f"{example_name}.did"

        if extract_candid(wasm_file, output_did):
            success_count += 1

    return success_count


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Extract Candid interfaces from IC WASM files"
    )
    parser.add_argument(
        "--wasm",
        type=Path,
        help="Single WASM file to extract from",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Output .did file path (required with --wasm)",
    )
    parser.add_argument(
        "--bin-dir",
        type=Path,
        help="Directory containing _ic.wasm files",
    )
    parser.add_argument(
        "--examples-dir",
        type=Path,
        help="Root examples directory (required with --bin-dir)",
    )

    args = parser.parse_args()

    if args.wasm:
        if not args.output:
            parser.error("--output is required with --wasm")
        success = extract_candid(args.wasm, args.output)
        exit(0 if success else 1)
    elif args.bin_dir:
        if not args.examples_dir:
            parser.error("--examples-dir is required with --bin-dir")
        count = extract_candid_for_examples(args.bin_dir, args.examples_dir)
        console.print(f"\n[bold]Extracted {count} .did file(s)[/]")
        exit(0 if count > 0 else 1)
    else:
        parser.print_help()
        exit(1)
