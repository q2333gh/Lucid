#!/usr/bin/env python3
"""Test that canister can correctly read data from stable memory after upload."""

import os
import sys
import pathlib
import argparse

# Clear proxy env vars
for key in list(os.environ.keys()):
    if 'proxy' in key.lower():
        del os.environ[key]

sys.path.insert(0, str(pathlib.Path(__file__).parent))
sys.path.insert(0, str(pathlib.Path(__file__).parent.parent.parent.parent))

from utils.chunk_utils import DEFAULT_CHUNK_SIZE
from ic.agent import Agent
from ic.client import Client
from ic.identity import Identity
from ic.candid import encode, decode, Types
from ic.principal import Principal


def compute_local_hash(data: bytes) -> int:
    """Compute DJB2 hash locally to match canister implementation."""
    hash_val = 5381
    for byte in data:
        hash_val = ((hash_val << 5) + hash_val + byte) & 0xFFFFFFFF  # hash * 33 + c, keep 32-bit
    return hash_val


def create_test_file(size: int) -> bytes:
    """Create test data with known pattern."""
    pattern = b"TEST_DATA_PATTERN_" + str(size).encode() + b"_"
    data = (pattern * (size // len(pattern) + 1))[:size]
    return data


def upload_test_asset(client, canister_id: str, name: str, data: bytes):
    """Upload test asset in chunks."""
    chunk_size = DEFAULT_CHUNK_SIZE
    total_size = len(data)
    offset = 0
    chunk_num = 0
    
    print(f"\nUploading '{name}' ({total_size:,} bytes)")
    
    while offset < total_size:
        chunk = data[offset:offset + chunk_size]
        actual_size = len(chunk)
        
        args = [
            {"type": Types.Text, "value": name},
            {"type": Types.Vec(Types.Nat8), "value": list(chunk)}
        ]
        
        if hasattr(client, 'update_call'):
            # PocketIC
            cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
            client.update_call(cid, "load_asset", encode(args))
        else:
            # Agent
            client.update_raw(canister_id, "load_asset", encode(args))
        
        offset += actual_size
        chunk_num += 1
    
    print(f"  ✓ Uploaded {chunk_num} chunks")


def reset_assets(client, canister_id):
    """Reset all assets in canister."""
    if hasattr(client, 'update_call'):
        cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
        client.update_call(cid, "reset_assets", encode([]))
    else:
        client.update_raw(canister_id, "reset_assets", encode([]))


def get_asset_hash_from_canister(client, canister_id, name: str) -> int:
    """Get hash of asset from canister (computed by reading from stable memory)."""
    args = [{"type": Types.Text, "value": name}]
    
    if hasattr(client, 'query_call'):
        cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
        result = client.query_call(cid, "compute_asset_hash", encode(args))
    else:
        result = client.query_raw(canister_id, "compute_asset_hash", encode(args))
    
    decoded = decode(result)
    
    if decoded and len(decoded) > 0:
        return int(decoded[0]['value'])
    return 0


def get_asset_sample_from_canister(client, canister_id, name: str, offset: int, length: int) -> bytes:
    """Get sample bytes from asset (read from stable memory)."""
    args = [
        {"type": Types.Text, "value": name},
        {"type": Types.Nat, "value": offset},
        {"type": Types.Nat, "value": length}
    ]
    
    if hasattr(client, 'query_call'):
        cid = canister_id if isinstance(canister_id, Principal) else Principal.from_str(canister_id)
        result = client.query_call(cid, "get_asset_sample", encode(args))
    else:
        result = client.query_raw(canister_id, "get_asset_sample", encode(args))
    
    decoded = decode(result)
    
    if decoded and len(decoded) > 0:
        blob = decoded[0]['value']
        return bytes(blob)
    return b""


def verify_asset_readback(client, canister_id: str, name: str, original_data: bytes) -> bool:
    """Verify asset can be read back from stable memory correctly."""
    print(f"\n=== Verifying '{name}' readback from stable memory ===")
    
    # 1. Verify hash
    print("\n1. Computing and comparing hash...")
    local_hash = compute_local_hash(original_data)
    canister_hash = get_asset_hash_from_canister(client, canister_id, name)
    
    print(f"   Local hash:    {local_hash:#010x} ({local_hash})")
    print(f"   Canister hash: {canister_hash:#010x} ({canister_hash})")
    
    if local_hash != canister_hash:
        print(f"   ✗ HASH MISMATCH")
        return False
    print(f"   ✓ Hash matches")
    
    # 2. Verify sample reads at different offsets
    print("\n2. Verifying sample reads from stable memory...")
    test_samples = [
        (0, min(1024, len(original_data))),  # First 1KB
        (len(original_data) // 2, min(1024, len(original_data) // 2)),  # Middle 1KB
        (max(0, len(original_data) - 1024), min(1024, len(original_data))),  # Last 1KB
    ]
    
    for offset, length in test_samples:
        expected = original_data[offset:offset + length]
        actual = get_asset_sample_from_canister(client, canister_id, name, offset, length)
        
        if expected != actual:
            print(f"   ✗ Sample mismatch at offset {offset}, length {length}")
            print(f"     Expected first 32 bytes: {expected[:32].hex()}")
            print(f"     Got first 32 bytes:      {actual[:32].hex()}")
            return False
        print(f"   ✓ Sample matches at offset {offset:,}, length {length:,}")
    
    print(f"\n✓ ALL VERIFICATIONS PASSED: Data correctly stored and readable from stable memory")
    return True


def test_single_chunk_readback(client, canister_id: str):
    """Test 1-chunk file upload and readback."""
    print("\n" + "="*70)
    print("TEST 1: Single Chunk Upload and Stable Memory Read (1 MB)")
    print("="*70)
    
    print("\nResetting assets...")
    reset_assets(client, canister_id)
    
    # Create 1 MB test file
    test_size = 1 * 1024 * 1024
    test_data = create_test_file(test_size)
    test_name = "test_single_readback"
    
    print(f"\nTest file: {test_size:,} bytes ({test_size / 1024 / 1024:.2f} MB)")
    
    # Upload
    upload_test_asset(client, canister_id, test_name, test_data)
    
    # Verify readback
    success = verify_asset_readback(client, canister_id, test_name, test_data)
    
    if success:
        print("\n✓ TEST 1 PASSED: Canister correctly reads 1-chunk file from stable memory")
    else:
        print("\n✗ TEST 1 FAILED: Canister cannot correctly read data from stable memory")
    
    return success


def test_two_chunks_readback(client, canister_id: str):
    """Test 2-chunk file upload and readback."""
    print("\n" + "="*70)
    print("TEST 2: Two Chunks Upload and Stable Memory Read (3 MB)")
    print("="*70)
    
    print("\nResetting assets...")
    reset_assets(client, canister_id)
    
    # Create 3 MB test file
    test_size = 3 * 1024 * 1024
    test_data = create_test_file(test_size)
    test_name = "test_two_readback"
    
    expected_chunks = (test_size + DEFAULT_CHUNK_SIZE - 1) // DEFAULT_CHUNK_SIZE
    print(f"\nTest file: {test_size:,} bytes ({test_size / 1024 / 1024:.2f} MB)")
    print(f"Expected chunks: {expected_chunks}")
    
    # Upload
    upload_test_asset(client, canister_id, test_name, test_data)
    
    # Verify readback
    success = verify_asset_readback(client, canister_id, test_name, test_data)
    
    if success:
        print("\n✓ TEST 2 PASSED: Canister correctly reads and reassembles 2-chunk file from stable memory")
    else:
        print("\n✗ TEST 2 FAILED: Canister cannot correctly read/reassemble data from stable memory")
    
    return success


def main():
    """Run all stable memory read verification tests."""
    parser = argparse.ArgumentParser(description='Test stable memory read correctness')
    parser.add_argument('--use-dfx', action='store_true', 
                       help='Use dfx replica instead of PocketIC (default: PocketIC)')
    args = parser.parse_args()
    
    print("\nSTABLE MEMORY READ CORRECTNESS TEST SUITE")
    print(f"Default chunk size: {DEFAULT_CHUNK_SIZE:,} bytes ({DEFAULT_CHUNK_SIZE / 1024 / 1024:.2f} MB)")
    print(f"Replica mode: {'dfx' if args.use_dfx else 'PocketIC'}")
    print("\nThis test verifies that the canister can correctly:")
    print("  1. Upload data to stable memory in chunks")
    print("  2. Read data back from stable memory")
    print("  3. Compute correct hash of stored data")
    print("  4. Read arbitrary byte ranges from stored data")
    
    pic = None
    
    if args.use_dfx:
        # Use dfx replica
        import subprocess
        print("\n[Using dfx replica]")
        canister_id = subprocess.run(
            ['dfx', 'canister', 'id', 'llm_c'],
            cwd=str(pathlib.Path(__file__).parent.parent),
            capture_output=True,
            text=True,
            check=True
        ).stdout.strip()
        client = Agent(Identity(), Client(url='http://127.0.0.1:4943'))
    else:
        # Use PocketIC (default)
        print("\n[Using PocketIC]")
        from test.support.test_support_pocketic import install_example_canister
        
        pic, canister_id = install_example_canister("llm_c", auto_build=False)
        client = pic  # Use PocketIC object directly
        print(f"Canister ID: {canister_id}")
    
    results = []
    
    try:
        results.append(("Single Chunk Read", test_single_chunk_readback(client, canister_id)))
    except Exception as e:
        print(f"\n✗ TEST 1 EXCEPTION: {e}")
        import traceback
        traceback.print_exc()
        results.append(("Single Chunk Read", False))
    
    try:
        results.append(("Two Chunks Read", test_two_chunks_readback(client, canister_id)))
    except Exception as e:
        print(f"\n✗ TEST 2 EXCEPTION: {e}")
        import traceback
        traceback.print_exc()
        results.append(("Two Chunks Read", False))
    
    # Summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    for name, passed in results:
        status = "✓ PASSED" if passed else "✗ FAILED"
        print(f"{name:30s}: {status}")
    
    all_passed = all(r[1] for r in results)
    print(f"\nOverall: {'✓ ALL TESTS PASSED' if all_passed else '✗ SOME TESTS FAILED'}")
    
    if all_passed:
        print("\n🎉 VERIFICATION COMPLETE:")
        print("   - Canister can correctly write to stable memory")
        print("   - Canister can correctly read from stable memory")
        print("   - Data integrity is maintained across chunk boundaries")
    
    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
