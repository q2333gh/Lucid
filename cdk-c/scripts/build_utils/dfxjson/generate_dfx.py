import json
import os
from pathlib import Path
from typing import Optional, List


def generate_dfx_json(
    canister_name: str, wasm_path: str, did_path: str, output_dir: str
):
    """
    Generates a dfx.json file for a specific canister.
    """
    dfx_content = {
        "canisters": {
            canister_name: {
                "candid": did_path,
                "wasm": wasm_path,
                "type": "custom",
                "metadata": [{"name": "candid:service"}],
            }
        },
        "defaults": {"build": {"args": "", "packtool": ""}},
        "output_env_file": ".env",
        "version": 1,
    }

    output_path = Path(output_dir) / "dfx.json"

    try:
        with open(output_path, "w") as f:
            json.dump(dfx_content, f, indent=2)
        print(f"Successfully generated {output_path}")
    except Exception as e:
        print(f"Error generating dfx.json: {e}")


def auto_generate_dfx(
    bin_dir: Path, examples_dir: Path, examples: Optional[List[str]] = None
):
    """
    Simplified version:
    1. Scan examples directory.
    2. For each example, look for a local .wasm file.
    3. If found, assume it's the target canister and generate dfx.json using local paths.

    Args:
        bin_dir: Directory containing built .wasm files (not used in current implementation)
        examples_dir: Root examples directory
        examples: Optional list of example names to process (None = all)
    """
    print(" Generating dfx.json configurations...")

    for example_dir in examples_dir.iterdir():
        if not example_dir.is_dir():
            continue

        # Filter by examples if specified
        if examples is not None:
            if example_dir.name not in examples:
                continue

        wasm_files = list(example_dir.glob("*.wasm"))
        if not wasm_files:
            continue

        selected_wasm = None
        for w in wasm_files:
            if "_optimized.wasm" in w.name:
                selected_wasm = w
                break
        if not selected_wasm:
            selected_wasm = wasm_files[0]

        canister_name = example_dir.name

        did_file = None
        did_candidates = [
            example_dir / f"{canister_name}.did",
            example_dir
            / f"{selected_wasm.stem.replace('_optimized', '').replace('_ic', '')}.did",
        ]

        for cand in did_candidates:
            if cand.exists():
                did_file = cand
                break

        if did_file:
            print(f" Generating dfx.json for {canister_name} in {example_dir.name}...")
            generate_dfx_json(
                canister_name=canister_name,
                wasm_path=selected_wasm.name,
                did_path=did_file.name,
                output_dir=str(example_dir),
            )


if __name__ == "__main__":
    import sys

    if len(sys.argv) >= 4:
        generate_dfx_json(
            sys.argv[1],
            sys.argv[2],
            sys.argv[3],
            sys.argv[4] if len(sys.argv) > 4 else ".",
        )
    else:
        print(
            "Usage: python generate_dfx.py <canister_name> <wasm_path> <did_path> [output_dir]"
        )
