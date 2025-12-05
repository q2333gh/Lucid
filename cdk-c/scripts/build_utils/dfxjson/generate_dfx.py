import json
import os
from pathlib import Path


def generate_dfx_json(
    canister_name: str, wasm_path: str, did_path: str, output_dir: str
):
    """
    Generates a dfx.json file for a specific canister.

    Args:
        canister_name: The name of the canister (e.g., 'greet').
        wasm_path: The path to the WASM file, relative to the dfx.json location.
        did_path: The path to the Candid (.did) file, relative to the dfx.json location.
        output_dir: The directory where dfx.json should be created.
    """

    # Template structure based on the user provided example
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


if __name__ == "__main__":
    # Example usage (for testing the script directly)
    # You would typically call this function from your main build script
    import sys

    if len(sys.argv) >= 4:
        name = sys.argv[1]
        wasm = sys.argv[2]
        did = sys.argv[3]
        out = sys.argv[4] if len(sys.argv) > 4 else "."
        generate_dfx_json(name, wasm, did, out)
    else:
        print(
            "Usage: python generate_dfx.py <canister_name> <wasm_path> <did_path> [output_dir]"
        )
