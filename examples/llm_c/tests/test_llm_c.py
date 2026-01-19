#!/usr/bin/env python3
"""Build, deploy and smoke-test the llm_c canister.

Supports two replica modes:
  - PocketIC (default): Automatic setup and management
  - dfx (--use-dfx): Manual dfx replica (must be running)
"""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys
import time
from typing import Any, Callable

from ic.agent import Agent
from ic.client import Client
from ic.identity import Identity
from ic.candid import encode, Types, decode
from ic.principal import Principal

# Add parent directory to path for imports
SCRIPT_DIR = pathlib.Path(__file__).resolve().parent.parent  # examples/llm_c/
REPO_ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(SCRIPT_DIR / "tests"))
sys.path.insert(0, str(REPO_ROOT))

from utils.chunk_utils import DEFAULT_CHUNK_SIZE, chunk_sizes


def configure_httpx_timeout(timeout_s: float) -> None:
    """Force a longer timeout for httpx calls used by ic.client."""
    import ic.client as ic_client
    import httpx

    def wrap(fn: Callable[..., httpx.Response]) -> Callable[..., httpx.Response]:
        def inner(*args: Any, **kwargs: Any) -> httpx.Response:
            kwargs.setdefault("timeout", timeout_s)
            return fn(*args, **kwargs)

        return inner

    ic_client.httpx.post = wrap(httpx.post)
    ic_client.httpx.get = wrap(httpx.get)


def run_build() -> None:
    """Build the llm_c WASM via the shared build script."""
    print("Building llm_c WASM...")
    subprocess.run(
        [sys.executable, "build.py", "--icwasm", "--examples", "llm_c"],
        check=True,
        cwd=REPO_ROOT,
    )


def deploy_canister() -> None:
    """Deploy the freshly built canister via dfx."""
    print("Deploying the llm_c canister...")
    subprocess.run(
        ["dfx", "deploy", "llm_c", "--no-wallet"],
        check=True,
        cwd=SCRIPT_DIR,
    )


def query_canister_id() -> str:
    """Read the local deployment's canister id for llm_c."""
    result = subprocess.run(
        ["dfx", "canister", "id", "llm_c"],
        cwd=SCRIPT_DIR,
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.strip()


def wait_for_replica(max_attempts: int = 20, delay_s: float = 1.0) -> None:
    """Wait for the local replica to accept requests."""
    for attempt in range(1, max_attempts + 1):
        try:
            subprocess.run(
                ["dfx", "ping"],
                cwd=SCRIPT_DIR,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=True,
            )
            return
        except subprocess.CalledProcessError:
            print(f"Replica not ready (attempt {attempt}/{max_attempts})")
            time.sleep(delay_s)
    raise RuntimeError("Replica did not become ready in time")

# TODO query icp doc for how to load binary data to canister. maybe just diff of how canister parse did data.
# cause its input is just raw binary data before candid parse it .
# so tell candid dont encode it and dont decode it.
# keep update payload under 2MiB ingress limit (leave generous headroom)
CHUNK_SIZE = 1_900_000


def load_asset(client, canister_id, name: str, path: pathlib.Path) -> None:
    """Load a single asset file into stable memory via the canister API.
    
    Args:
        client: Agent or PocketIC instance
        canister_id: str or Principal
        name: Asset name
        path: Path to asset file
    """
    if not path.exists():
        raise FileNotFoundError(f"{path} does not exist")
    print(f"Loading asset {name} from {path}...")
    chunk_index = 0
    total_bytes = 0
    with path.open("rb") as asset_file:
        while True:
            chunk = asset_file.read(CHUNK_SIZE)
            if not chunk:
                break
            chunk_index += 1
            args = [
                {"type": Types.Text, "value": name},
                {"type": Types.Vec(Types.Nat8), "value": list(chunk)},
            ]
            
            # Support both Agent and PocketIC
            if hasattr(client, 'update_call'):
                # PocketIC
                cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
                client.update_call(cid, "load_asset", encode(args))
            else:
                # Agent
                client.update_raw(canister_id, "load_asset", encode(args))
            
            total_bytes += len(chunk)
            print(f"...{name}: sent chunk {chunk_index} ({len(chunk)} bytes)")
    print(f"...{name} loaded ({total_bytes} bytes in {chunk_index} chunks)")


def is_valid_checkpoint(path: pathlib.Path) -> bool:
    try:
        with path.open("rb") as f:
            header = f.read(8)
        if len(header) < 8:
            return False
        magic, version = int.from_bytes(header[:4], "little"), int.from_bytes(
            header[4:], "little"
        )
        return magic == 20240326 and version == 1
    except OSError:
        return False


def is_valid_gguf(path: pathlib.Path) -> bool:
    try:
        with path.open("rb") as f:
            header = f.read(4)
        return header == b"GGUF"
    except OSError:
        return False


def resolve_path(path_str: str | None) -> pathlib.Path | None:
    if not path_str:
        return None
    path = pathlib.Path(path_str)
    if path.is_absolute():
        return path
    repo_candidate = REPO_ROOT / path
    if repo_candidate.exists():
        return repo_candidate
    return SCRIPT_DIR / path


def pick_checkpoint_path(checkpoint: pathlib.Path | None) -> pathlib.Path:
    if checkpoint:
        if checkpoint.exists() and is_valid_checkpoint(checkpoint):
            return checkpoint
        raise FileNotFoundError(f"{checkpoint} is not a valid checkpoint")
    candidates = [
        SCRIPT_DIR / "assets" / "gpt2_124M.bin",
    ]
    for path in candidates:
        if path.exists() and is_valid_checkpoint(path):
            return path
    raise FileNotFoundError(
        "valid checkpoint not found (expected header magic 20240326  (little endian: c6 d7 34 01))"
    )


def asset_exists(client, canister_id, name: str) -> bool:
    encoded_args = encode([{"type": Types.Text, "value": name}])
    
    if hasattr(client, 'query_call'):
        # PocketIC
        cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
        result = client.query_call(cid, "asset_exists", encoded_args)
        decoded = decode(result)
        if decoded and len(decoded) > 0:
            return int(decoded[0]['value']) != 0
    else:
        # Agent
        result = client.query_raw(
            canister_id, "asset_exists", encoded_args, return_type=[Types.Nat64]
        )
        if isinstance(result, list) and result:
            value = result[0].get("value") if isinstance(result[0], dict) else result[0]
            return int(value) != 0
    return False


def checkpoint_complete(client, canister_id) -> bool:
    if hasattr(client, 'query_call'):
        # PocketIC
        cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
        result = client.query_call(cid, "checkpoint_complete", encode([]))
        decoded = decode(result)
        if decoded and len(decoded) > 0:
            return int(decoded[0]['value']) != 0
    else:
        # Agent
        result = client.query_raw(
            canister_id,
            "checkpoint_complete",
            encode([]),
            return_type=[Types.Nat64],
        )
        if isinstance(result, list) and result:
            value = result[0].get("value") if isinstance(result[0], dict) else result[0]
            return int(value) != 0
    return False


def verify_asset_complete(client, canister_id, name: str) -> bool:
    """Verify that an asset exists and is complete (for checkpoint, also check completeness)."""
    if not asset_exists(client, canister_id, name):
        return False
    if name == "checkpoint":
        return checkpoint_complete(client, canister_id)
    return True


def upload_assets(
    client,
    canister_id,
    checkpoint: pathlib.Path | None,
    gguf: pathlib.Path | None,
    stories: pathlib.Path | None,
    skip_existing: bool,
) -> None:
    """Upload the checkpoint/gguf, vocab and merge files required for inference.
    
    Args:
        client: Agent or PocketIC instance
        canister_id: Canister ID (str or Principal)
        checkpoint: Optional checkpoint path override
        gguf: Optional gguf model path
        stories: Optional stories asset path
        skip_existing: If True, skip upload if asset exists and is complete
    """
    assets = []
    if gguf is not None:
        assets.append(("model", gguf))
    else:
        checkpoint_path = pick_checkpoint_path(checkpoint)
        assets.append(("checkpoint", checkpoint_path))

    assets.append(("vocab", SCRIPT_DIR / "assets" / "encoder.json"))
    assets.append(("merges", SCRIPT_DIR / "assets" / "vocab.bpe"))
    if stories:
        assets.append(("stories", stories))
    for name, path in assets:
        if skip_existing and verify_asset_complete(client, canister_id, name):
            print(f"Skipping asset {name}; already present and complete")
            continue
        load_asset(client, canister_id, name, path)


def reset_assets(client, canister_id) -> None:
    """Clear any previous asset table state to ensure contiguous uploads."""
    if hasattr(client, 'update_call'):
        # PocketIC
        cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
        client.update_call(cid, "reset_assets", encode([]))
    else:
        # Agent
        client.update_raw(canister_id, "reset_assets", encode([]))


def call_infer(
    client,
    canister_id,
    max_new: int | None,
    use_stories: bool,
    stories_offset: int,
    stories_length: int,
    use_step: bool,
) -> str:
    """Call the `infer_hello_update` update and return its textual response."""
    
    def _call_update(method: str, args):
        """Helper to call update methods on either Agent or PocketIC."""
        if hasattr(client, 'update_call'):
            # PocketIC
            cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
            result = client.update_call(cid, method, args)
            decoded = decode(result)
            if decoded and len(decoded) > 0:
                return decoded[0]['value']
            return ""
        else:
            # Agent
            result = client.update_raw(canister_id, method, args, return_type=[Types.Text])
            if isinstance(result, list) and result:
                return result[0]
            return ""
    
    if use_stories:
        if use_step:
            init_args = encode(
                [
                    {"type": Types.Nat32, "value": stories_offset},
                    {"type": Types.Nat32, "value": stories_length},
                    {"type": Types.Nat32, "value": max_new or 0},
                ]
            )
            print("Calling infer_stories_step_init...")
            _call_update("infer_stories_step_init", init_args)
            
            step_args = encode([{"type": Types.Nat32, "value": 1}])
            print("Calling infer_stories_step...")
            _call_update("infer_stories_step", step_args)
            print("Calling infer_stories_step_get...")
            if hasattr(client, 'query_call'):
                cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
                result = client.query_call(cid, "infer_stories_step_get", encode([]))
                decoded = decode(result)
                if decoded and len(decoded) > 0:
                    return decoded[0]['value']
                return ""
            result = client.query_raw(
                canister_id,
                "infer_stories_step_get",
                encode([]),
                return_type=[Types.Text],
            )
            if isinstance(result, list) and result:
                return result[0]
            return ""
        if max_new == 1 and stories_offset == 0 and stories_length == 64:
            method = "infer_stories_update"
            encoded_args = encode([])
        else:
            method = "infer_stories_update_limited"
            encoded_args = encode(
                [
                    {"type": Types.Nat32, "value": stories_offset},
                    {"type": Types.Nat32, "value": stories_length},
                    {"type": Types.Nat32, "value": max_new or 0},
                ]
            )
    elif max_new is None:
        method = "infer_hello_update"
        encoded_args = encode([])
    else:
        method = "infer_hello_update_limited"
        encoded_args = encode([{"type": Types.Nat32, "value": max_new}])
    
    print(f"Calling {method}...")
    return _call_update(method, encoded_args)


def verify_assets_loaded(client, canister_id) -> bool:
    """Call the `assets_loaded` query to confirm the canister sees the uploads."""
    print("Verifying assets_loaded query...")
    encoded_args = encode([])
    
    if hasattr(client, 'query_call'):
        # PocketIC
        cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
        result = client.query_call(cid, "assets_loaded", encoded_args)
        decoded = decode(result)
        if decoded and len(decoded) > 0:
            print("assets_loaded ->", decoded[0]['value'])
            return True
    else:
        # Agent
        result = client.query_raw(
            canister_id, "assets_loaded", encoded_args, return_type=[Types.Text]
        )
        if isinstance(result, list) and result:
            print("assets_loaded ->", result[0])
            return True
    
    print("assets_loaded replied with empty payload")
    return False


def ping_hello(client, canister_id) -> None:
    """Attempt a quick infer_hello query and log the outcome before full testing."""
    print("Querying hello once before asset upload...")
    try:
        encoded_args = encode([])
        
        if hasattr(client, 'query_call'):
            # PocketIC
            cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
            result = client.query_call(cid, "hello", encoded_args)
            decoded = decode(result)
            if decoded and len(decoded) > 0:
                print("hello probe ->", decoded[0]['value'])
            else:
                print("hello probe returned nothing")
        else:
            # Agent
            result = client.query_raw(
                canister_id, "hello", encoded_args, return_type=[Types.Text]
            )
            if isinstance(result, list) and result:
                print("hello probe ->", result[0])
            else:
                print("hello probe returned nothing")
    except Exception as exc:  # pragma: no cover - best effort probe
        print("hello probe failed:", exc)


def main() -> None:
    # Avoid proxies interfering with local replica calls.
    for proxy_var in (
        "HTTP_PROXY",
        "http_proxy",
        "HTTPS_PROXY",
        "https_proxy",
        "ALL_PROXY",
        "all_proxy",
    ):
        os.environ.pop(proxy_var, None)
  
    parser = argparse.ArgumentParser(
        description="Deploy and test the llm_c canister.",
        epilog="Default: dfx replica. Use --use-pocketic for isolated testing."
    )
    parser.add_argument(
        "--use-pocketic",
        action="store_true",
        help="Use PocketIC instead of dfx (automatic, isolated testing)",
    )
    parser.add_argument(
        "--replica-url",
        "-u",
        default=os.environ.get("LLM_TEST_REPLICA", "http://127.0.0.1:4943"),
        help="URL of the running replica (default: %(default)s)",
    )
    parser.add_argument(
        "--skip-build",
        "-sb",
        action="store_true",
        help="Skip the shared build step (assume wasm is already built)",
    )
    parser.add_argument(
        "--skip-deploy",
        "-sd",
        action="store_true",
        help="Skip `dfx deploy` (assume canister is already installed)",
    )
    parser.add_argument(
        "--reset-assets",
        "-ra",
        action="store_true",
        help="Force reset of asset table before uploads",
    )
    parser.add_argument(
        "--skip-upload",
        "-su",
        action="store_true",
        help="Skip asset upload (assume assets already loaded)",
    )
    parser.add_argument(
        "--checkpoint",
        "-c",
        default="examples/llm_c/assets/gpt2_124M.bin",
        help="Checkpoint file to upload (default: %(default)s)",
    )
    parser.add_argument(
        "--stories",
        "-s",
        default="examples/llm_c/assets/stories15M.bin",
        help="Stories token file to upload (default: %(default)s)",
    )
    parser.add_argument(
        "--no-stories",
        "-ns",
        action="store_true",
        help="Disable stories token prompt and use the hello prompt instead",
    )
    parser.add_argument(
        "--with-stories",
        action="store_true",
        help="Force stories upload even when GGUF mode is enabled",
    )
    parser.add_argument(
        "--stories-offset",
        "-so",
        type=int,
        default=0,
        help="Token offset into stories file (default: %(default)s)",
    )
    parser.add_argument(
        "--stories-length",
        "-sl",
        type=int,
        default=64,
        help="Token count from stories file (default: %(default)s)",
    )
    parser.add_argument(
        "--max-new",
        "-mn",
        type=int,
        default=64,
        help="Max new tokens for infer_hello_update_limited (default: %(default)s)",
    )
    parser.add_argument(
        "--no-step",
        "-nst",
        action="store_true",
        help="Disable step-mode inference for stories",
    )
    parser.add_argument(
        "--gguf",
        default="lib/tiny-gpt2-f16.gguf",
        help="GGUF model path to upload as 'model' (default: %(default)s)",
    )
    parser.add_argument(
        "--no-gguf",
        action="store_true",
        help="Disable GGUF upload even if the file exists",
    )
    parser.add_argument(
        "--http-timeout",
        "-ht",
        type=float,
        default=300.0,
        help="HTTP timeout seconds for canister calls (default: %(default)s)",
    )
    args = parser.parse_args()

    # Validate arguments
    if args.skip_upload and args.use_pocketic:
        print("⚠️  Warning: --skip-upload doesn't work with --use-pocketic mode.")
        print("   PocketIC creates fresh instances, so assets must be uploaded each time.")
        print("   Continuing without skipping upload...\n")
        args.skip_upload = False
    
    # Setup client and canister based on replica mode
    if args.use_pocketic:
        # PocketIC mode: automatic setup (good for clean tests)
        print("\n[Using PocketIC]")
        print("Mode: Fresh, isolated test environment")
        print("Note: Assets re-upload each run (PocketIC doesn't preserve state)")
        print("Tip:  Use default dfx mode for iterative testing with asset persistence\n")
        
        from test.support.test_support_pocketic import install_example_canister
        
        # Build if needed (PocketIC auto_build handles this, but respect skip flag)
        if not args.skip_build:
            run_build()
        
        # Install canister with PocketIC
        pic, canister_id = install_example_canister("llm_c", auto_build=False)
        client = pic
        print(f"Canister ID: {canister_id}")
    else:
        # dfx mode: manual replica (default, good for iterative development)
        print("\n[Using dfx replica (default)]")
        print("Mode: State-preserving replica for iterative testing")
        print("Usage: Assets upload once, preserved across test runs")
        print("Tip:  Use --skip-upload after first run for fast iteration\n")
        
        if not args.skip_build:
            run_build()
        
        if not args.skip_deploy:
            wait_for_replica()
            deploy_canister()
        
        canister_id = query_canister_id()
        
        configure_httpx_timeout(args.http_timeout)
        identity = Identity()
        client_obj = Client(url=args.replica_url)
        client = Agent(identity, client_obj)
    
    # Common test flow for both modes
    ping_hello(client, canister_id)
    
    checkpoint_path = resolve_path(args.checkpoint)
    gguf_path = None
    if not args.no_gguf:
        gguf_candidate = resolve_path(args.gguf)
        if gguf_candidate and gguf_candidate.exists() and is_valid_gguf(gguf_candidate):
            gguf_path = gguf_candidate
        elif gguf_candidate:
            print(f"⚠️  GGUF file not found/invalid: {gguf_candidate} (fallback to checkpoint)")
    stories_path = None if args.no_stories else resolve_path(args.stories)
    if gguf_path is not None and not args.with_stories:
        stories_path = None

    if gguf_path is None:
        if not checkpoint_path or not is_valid_checkpoint(checkpoint_path):
            raise FileNotFoundError(
                f"{checkpoint_path} is not a valid checkpoint"
            )
    
    # Handle asset upload
    if args.skip_upload:
        print("Skipping asset upload (--skip-upload specified)")
        # Verify assets are actually present
        if not verify_assets_loaded(client, canister_id):
            print("⚠️  Warning: Assets not found, but --skip-upload was specified")
            print("   Continuing anyway (inference will likely fail)...\n")
    else:
        assets_ready = False
        if not args.reset_assets:
            try:
                assets_ready = verify_assets_loaded(client, canister_id)
            except Exception:
                assets_ready = False

        reset_now = args.reset_assets
        if not reset_now:
            try:
                primary_asset = "model" if gguf_path is not None else "checkpoint"
                if not verify_asset_complete(client, canister_id, primary_asset):
                    if asset_exists(client, canister_id, primary_asset):
                        reset_now = True
            except Exception:
                reset_now = True

        if reset_now:
            reset_assets(client, canister_id)
            assets_ready = False

        if not assets_ready:
            print(f"\n{'='*70}")
            print("UPLOADING ASSETS")
            print(f"{'='*70}")
            if gguf_path is not None:
                print(f"GGUF:       {gguf_path.name} ({gguf_path.stat().st_size / 1024 / 1024:.1f} MB)")
            else:
                print(f"Checkpoint: {checkpoint_path.name} ({checkpoint_path.stat().st_size / 1024 / 1024:.1f} MB)")
            if stories_path:
                print(f"Stories:    {stories_path.name} ({stories_path.stat().st_size / 1024 / 1024:.1f} MB)")
            print(f"{'='*70}\n")
            
            upload_assets(
                client,
                canister_id,
                checkpoint_path,
                gguf_path,
                stories_path,
                skip_existing=not reset_now,
            )
            verify_assets_loaded(client, canister_id)
        else:
            print("✓ Assets already loaded and complete, skipping upload\n")
    
    output = call_infer(
        client,
        canister_id,
        args.max_new,
        stories_path is not None,
        args.stories_offset,
        args.stories_length,
        not args.no_step,
    )
    print(f"\n{'='*70}")
    print(f"INFERENCE RESULT")
    print(f"{'='*70}")
    print(f"{output}")
    print(f"{'='*70}")


if __name__ == "__main__":

    main()
