#!/usr/bin/env python3
"""
Test suite for types_example canister.

Tests all Candid type examples including:
- Simple query methods
- Multiple parameters
- Record types
- Variant types
- Complex nested types (vec, opt, record, variant)
- Multiple return values
"""

import sys
from pathlib import Path

# Add project root to path for imports
project_root = Path(__file__).resolve().parent.parent.parent
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

from ic.candid import Types, encode
from ic.principal import Principal
from test.support.test_support_pocketic import (
    decode_candid_text,
    install_example_canister,
)


def ensure_principal(canister_id) -> Principal:
    """Convert canister_id to Principal if needed"""
    if isinstance(canister_id, Principal):
        return canister_id
    return Principal.from_str(str(canister_id))


def test_greet(pic, canister_id) -> None:
    """Test greet: (text) -> (text) query"""
    print("\n=== Test: greet ===")
    principal_id = ensure_principal(canister_id)

    # Call greet with a name
    params = [{"type": Types.Text, "value": "World"}]
    payload = encode(params)
    response_bytes = pic.query_call(principal_id, "greet", payload)
    decoded = decode_candid_text(response_bytes)
    print(f"greet('World') -> {decoded!r}")
    assert "Hello" in decoded or "World" in decoded, f"Unexpected response: {decoded}"


def test_add_user(pic, canister_id) -> None:
    """Test add_user: (text, nat, bool) -> (text)"""
    print("\n=== Test: add_user ===")

    # Call add_user with multiple parameters
    params = [
        {"type": Types.Text, "value": "Alice"},
        {"type": Types.Nat, "value": 25},
        {"type": Types.Bool, "value": True},
    ]
    payload = encode(params)
    principal_id = ensure_principal(canister_id)
    response_bytes = pic.update_call(principal_id, "add_user", payload)
    decoded = decode_candid_text(response_bytes)
    print(f"add_user('Alice', 25, True) -> {decoded!r}")
    assert (
        "Alice" in decoded and "successfully" in decoded
    ), f"Unexpected response: {decoded}"


def test_set_address(pic, canister_id) -> None:
    """Test set_address: (text, record) -> (text)"""
    print("\n=== Test: set_address ===")
    principal_id = ensure_principal(canister_id)

    # Define Record type using Types.Record
    address_type = Types.Record(
        {"street": Types.Text, "city": Types.Text, "zip": Types.Nat}
    )

    # Encode parameters with proper format
    params = [
        {"type": Types.Text, "value": "Bob"},
        {
            "type": address_type,
            "value": {"street": "123 Main St", "city": "San Francisco", "zip": 94102},
        },
    ]

    try:
        payload = encode(params)
        response_bytes = pic.update_call(principal_id, "set_address", payload)
        decoded = decode_candid_text(response_bytes)
        print(
            f"set_address('Bob', {{street: '...', city: '...', zip: 94102}}) -> {decoded!r}"
        )
        assert (
            "Bob" in decoded and "successfully" in decoded
        ), f"Unexpected response: {decoded}"
    except Exception as e:
        print(f"Error: {e}")
        raise


def test_get_address(pic, canister_id) -> None:
    """Test get_address: (text) -> (opt record) query"""
    print("\n=== Test: get_address ===")

    params = [{"type": Types.Text, "value": "Alice"}]
    payload = encode(params)
    principal_id = ensure_principal(canister_id)
    response_bytes = pic.query_call(principal_id, "get_address", payload)
    decoded = decode_candid_text(response_bytes)
    print(f"get_address('Alice') -> {decoded!r}")
    # Should return an optional address record
    assert response_bytes is not None


def test_set_status(pic, canister_id) -> None:
    """Test set_status: (text, variant) -> (text)"""
    print("\n=== Test: set_status ===")
    principal_id = ensure_principal(canister_id)

    # Define Variant type using Types.Variant
    status_type = Types.Variant(
        {"Active": Types.Null, "Inactive": Types.Null, "Banned": Types.Text}
    )

    # Test with Active variant (no data)
    params = [
        {"type": Types.Text, "value": "user1"},
        {"type": status_type, "value": {"Active": None}},
    ]

    try:
        payload = encode(params)
        response_bytes = pic.update_call(principal_id, "set_status", payload)
        decoded = decode_candid_text(response_bytes)
        print(f"set_status('user1', Active) -> {decoded!r}")
        assert (
            "user1" in decoded and "successfully" in decoded
        ), f"Unexpected response: {decoded}"

        # Test with Banned variant (with text data)
        params2 = [
            {"type": Types.Text, "value": "user2"},
            {"type": status_type, "value": {"Banned": "spam detected"}},
        ]
        payload2 = encode(params2)
        response_bytes2 = pic.update_call(principal_id, "set_status", payload2)
        decoded2 = decode_candid_text(response_bytes2)
        print(f"set_status('user2', Banned('spam detected')) -> {decoded2!r}")
        assert (
            "user2" in decoded2 and "successfully" in decoded2
        ), f"Unexpected response: {decoded2}"
    except Exception as e:
        print(f"Error: {e}")
        raise


def test_get_status(pic, canister_id) -> None:
    """Test get_status: (text) -> (variant) query"""
    print("\n=== Test: get_status ===")

    params = [{"type": Types.Text, "value": "user1"}]
    payload = encode(params)
    principal_id = ensure_principal(canister_id)
    response_bytes = pic.query_call(principal_id, "get_status", payload)
    decoded = decode_candid_text(response_bytes)
    print(f"get_status('user1') -> {decoded!r}")
    # Should return a variant (likely Banned based on implementation)
    assert response_bytes is not None


def test_create_profiles(pic, canister_id) -> None:
    """Test create_profiles: (vec record) -> (vec variant)"""
    print("\n=== Test: create_profiles ===")

    # Build a simple profile record using dictionary format
    profile = {
        "id": 1,
        "name": "Test User",
        "emails": ["test@example.com"],
        "age": 30,  # Will be wrapped in opt by encode
        "status": {"Active": None},  # Variant
    }

    principal_id = ensure_principal(canister_id)

    # Define nested types
    status_type = Types.Variant(
        {"Active": Types.Null, "Inactive": Types.Null, "Banned": Types.Text}
    )

    profile_type = Types.Record(
        {
            "id": Types.Nat,
            "name": Types.Text,
            "emails": Types.Vec(Types.Text),
            "age": Types.Opt(Types.Nat),
            "status": status_type,
        }
    )

    # Create a profile value
    profile_value = {
        "id": 1,
        "name": "Test User",
        "emails": ["test@example.com"],
        "age": [30],  # Opt: [value] means Some(value)
        "status": {"Active": None},  # Variant
    }

    # Encode vec of profiles
    params = [{"type": Types.Vec(profile_type), "value": [profile_value]}]

    try:
        payload = encode(params)
        response_bytes = pic.update_call(principal_id, "create_profiles", payload)
        decoded = decode_candid_text(response_bytes)
        print(f"create_profiles([profile]) -> {decoded!r}")
        # Should return vec of Result variants (Ok/Err)
        assert response_bytes is not None
    except Exception as e:
        print(f"Error: {e}")
        raise


def test_find_profile(pic, canister_id) -> None:
    """Test find_profile: (nat) -> (opt record) query"""
    print("\n=== Test: find_profile ===")

    params = [{"type": Types.Nat, "value": 7}]
    payload = encode(params)
    principal_id = ensure_principal(canister_id)
    response_bytes = pic.query_call(principal_id, "find_profile", payload)
    decoded = decode_candid_text(response_bytes)
    print(f"find_profile(7) -> {decoded!r}")
    # Should return an optional profile record
    assert response_bytes is not None


def test_stats(pic, canister_id) -> None:
    """Test stats: () -> (nat, vec nat, text) query"""
    print("\n=== Test: stats ===")

    # No parameters
    params = []
    payload = encode(params)
    principal_id = ensure_principal(canister_id)
    response_bytes = pic.query_call(principal_id, "stats", payload)
    decoded = decode_candid_text(response_bytes)
    print(f"stats() -> {decoded!r}")
    # Should return tuple: (nat, vec nat, text)
    assert response_bytes is not None
    # The decoded text should contain some indication of the response
    assert len(response_bytes) > 0


def run_all_tests() -> None:
    """Run all test cases"""
    print("=" * 60)
    print("Running types_example test suite")
    print("=" * 60)

    # Build and install canister once for all tests
    print("\n[Setup] Building and installing canister...")
    pic, canister_id = install_example_canister("types_example", auto_build=True)
    print(f"[Setup] Canister installed: {canister_id}\n")

    # List of all test functions
    tests = [
        ("greet", test_greet),
        ("add_user", test_add_user),
        ("set_address", test_set_address),
        ("get_address", test_get_address),
        ("set_status", test_set_status),
        ("get_status", test_get_status),
        ("create_profiles", test_create_profiles),
        ("find_profile", test_find_profile),
        ("stats", test_stats),
    ]

    passed = 0
    failed = 0

    for test_name, test_func in tests:
        try:
            test_func(pic, canister_id)
            passed += 1
        except Exception as e:
            failed += 1
            print(f"\n⚠️  Test '{test_name}' failed: {e}")
            # Continue with other tests instead of stopping

    print("\n" + "=" * 60)
    print(f"Test Summary: {passed} passed, {failed} failed")
    print("=" * 60)

    if failed > 0:
        print(f"\n⚠️  {failed} test(s) failed. Check the output above for details.")
        sys.exit(1)
    else:
        print("\n✅ All tests completed successfully!")


if __name__ == "__main__":
    run_all_tests()
