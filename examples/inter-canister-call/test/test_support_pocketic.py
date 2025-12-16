#!/usr/bin/env python3
"""
PocketIC helpers for example canisters.

Responsibilities:
  - locate / configure the pocket-ic binary (POCKET_IC_BIN)
  - build and install an example canister into a PocketIC instance
  - provide simple helpers to call a method and (optionally) assert on output
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Optional

from pocket_ic import PocketIC
from ic.candid import encode
from ic.principal import Principal
from test_support_build import build_example_ic_wasm, get_wasm_and_did_paths


# TODO performance: 1. make pocket-it asnyc init to prepare for service call, because it takes ~3s
# TODO 2.give a flag to mannually skip build, because autoskip in build.py is not impl yet
def setup_pocketic_binary() -> None:
    """
    Ensure POCKET_IC_BIN is set, preferring a `pocket-ic` binary
    next to this test directory, then in CWD, otherwise leave as-is.
    """
    script_dir = Path(__file__).parent.resolve()
    candidate = script_dir / "pocket-ic"

    if "POCKET_IC_BIN" not in os.environ:
        if candidate.exists():
            os.environ["POCKET_IC_BIN"] = str(candidate)
            print(f"[pocket-ic] Using local binary: {candidate}")
        else:
            cwd_candidate = Path.cwd() / "pocket-ic"
            if cwd_candidate.exists():
                os.environ["POCKET_IC_BIN"] = str(cwd_candidate)
                print(f"[pocket-ic] Using CWD binary: {cwd_candidate}")
            else:
                print(
                    "[pocket-ic] Warning: pocket-ic binary not found next to tests or in CWD.\n"
                    "            Set POCKET_IC_BIN or place a `pocket-ic` binary accordingly."
                )


def install_example_canister(
    example_name: str,
    *,
    auto_build: bool = True,
) -> tuple[PocketIC, str]:
    """
    Build (optionally) and install an example canister into PocketIC.

    Returns:
      (PocketIC instance, canister_id_text)
    """
    setup_pocketic_binary()

    if auto_build:
        build_example_ic_wasm(example_name)

    wasm_path, did_path = get_wasm_and_did_paths(example_name)

    print(f"[install] WASM: {wasm_path}")
    print(f"[install] DID:  {did_path}")

    pic = PocketIC()
    print("[install] PocketIC initialized")

    with open(wasm_path, "rb") as f:
        wasm_module = f.read()
    with open(did_path, "r") as f:
        candid_interface = f.read()

    canister = pic.create_and_install_canister_with_candid(
        candid=candid_interface,
        wasm_module=wasm_module,
    )
    canister_id = canister.canister_id
    print(f"[install] Canister ID: {canister_id}")
    return pic, canister_id


def install_multiple_examples(
    example_names: list[str],
    *,
    auto_build: bool = True,
) -> tuple[PocketIC, dict[str, Principal]]:
    """
    Build (optionally) and install multiple example canisters into a *single*
    PocketIC instance.

    Returns:
      (PocketIC instance, {example_name: canister_id_principal})
    """
    if not example_names:
        raise ValueError("install_multiple_examples: example_names must not be empty")

    setup_pocketic_binary()

    if auto_build:
        for name in example_names:
            build_example_ic_wasm(name)

    wasm_did_paths: dict[str, tuple[Path, Path]] = {}
    for name in example_names:
        wasm_path, did_path = get_wasm_and_did_paths(name)
        wasm_did_paths[name] = (wasm_path, did_path)

    pic = PocketIC()
    print("[install-multi] PocketIC initialized")

    ids: dict[str, Principal] = {}
    for name, (wasm_path, did_path) in wasm_did_paths.items():
        print(f"[install-multi] Installing '{name}'")
        with open(wasm_path, "rb") as f:
            wasm_module = f.read()
        with open(did_path, "r") as f:
            candid_interface = f.read()

        canister = pic.create_and_install_canister_with_candid(
            candid=candid_interface,
            wasm_module=wasm_module,
        )
        canister_id = canister.canister_id
        # Normalize to Principal explicitly for clarity
        if isinstance(canister_id, Principal):
            principal_id = canister_id
        else:
            principal_id = Principal.from_str(str(canister_id))

        ids[name] = principal_id
        print(f"[install-multi]  -> {name} ID: {principal_id}")

    return pic, ids


def decode_candid_text(response_bytes: bytes) -> str:
    """
    Simple heuristic Candid text decoder used for assertions/logging.
    """
    if not isinstance(response_bytes, (bytes, bytearray)):
        return str(response_bytes)

    data = bytes(response_bytes)

    try:
        if data.startswith(b"DIDL"):
            import re

            text_content = re.search(b"[\\x20-\\x7E]+$", data)
            if text_content:
                return text_content.group(0).decode("utf-8", errors="ignore")
    except Exception:
        pass

    try:
        decoded = data.decode("utf-8", errors="ignore")
        import string

        return "".join(ch for ch in decoded if ch in string.printable).strip()
    except Exception:
        return str(response_bytes)


def query_and_expect_substr(
    pic: PocketIC,
    canister_id: str,
    method: str,
    expected_substr: Optional[str] = None,
) -> str:
    """
    Perform an UPDATE call with no arguments and (optionally) assert that
    the response contains a given substring.

    Returns the (approximately) decoded text response.
    """
    print(f"\n=== update {method}() ===")
    payload = encode([])  # Empty args
    response_bytes = pic.update_call(Principal.from_str(canister_id), method, payload)

    print(f"Raw response: {response_bytes}")
    decoded = decode_candid_text(response_bytes)
    print(f"Decoded text (approx): {decoded!r}")

    if expected_substr is not None:
        if expected_substr in decoded:
            print(f"✓ contains expected substring: {expected_substr!r}")
        else:
            raise AssertionError(
                f"Expected substring {expected_substr!r} not found in response {decoded!r}"
            )

    return decoded
