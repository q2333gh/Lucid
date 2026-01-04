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

Usage:
    # Run all tests with pytest
    pytest examples/types_example/te_test.py

    # Run a specific test
    pytest examples/types_example/te_test.py::test_greet

    # Run tests matching a pattern
    pytest examples/types_example/te_test.py -k greet

    # Run all tests (backward compatibility)
    python3 examples/types_example/te_test.py
"""

import sys
from pathlib import Path

# Add project root to path for imports
project_root = Path(__file__).resolve().parent.parent.parent
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

import pytest
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


@pytest.fixture(scope="module")
def pic_and_canister():
    """Pytest fixture: Build and install canister once for all tests in this module."""
    print("\n[Setup] Building and installing canister...")
    pic, canister_id = install_example_canister("types_example", auto_build=True)
    print(f"[Setup] Canister installed: {canister_id}\n")
    return pic, canister_id


@pytest.fixture
def pic(pic_and_canister):
    """Pytest fixture: Extract PocketIC instance."""
    return pic_and_canister[0]


@pytest.fixture
def canister_id(pic_and_canister):
    """Pytest fixture: Extract canister ID."""
    return pic_and_canister[1]


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

    payload = encode(params)
    response_bytes = pic.update_call(principal_id, "set_address", payload)
    decoded = decode_candid_text(response_bytes)
    print(
        f"set_address('Bob', {{street: '...', city: '...', zip: 94102}}) -> {decoded!r}"
    )
    assert (
        "Bob" in decoded and "successfully" in decoded
    ), f"Unexpected response: {decoded}"


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

    payload = encode(params)
    response_bytes = pic.update_call(principal_id, "create_profiles", payload)
    decoded = decode_candid_text(response_bytes)
    print(f"create_profiles([profile]) -> {decoded!r}")
    # Should return vec of Result variants (Ok/Err)
    assert response_bytes is not None


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


def test_complex_test(pic, canister_id) -> None:
    """Test complex_test: (vec record with all types, double nesting) -> (vec variant)"""
    print("\n=== Test: complex_test ===")
    principal_id = ensure_principal(canister_id)

    # Define nested types for input
    # Status variant
    status_type = Types.Variant(
        {"Active": Types.Null, "Inactive": Types.Null, "Banned": Types.Text}
    )

    # Details record: { count: nat; items: vec nat }
    details_type = Types.Record({"count": Types.Nat, "items": Types.Vec(Types.Nat)})

    # Metadata record: { tags: vec text; status: variant; details: record }
    metadata_type = Types.Record(
        {
            "tags": Types.Vec(Types.Text),
            "status": status_type,
            "details": details_type,
        }
    )

    # Item record: { value: text; score: nat }
    item_type = Types.Record({"value": Types.Text, "score": Types.Nat})

    # Main input record: { id: nat; name: text; active: bool; metadata: opt record; items: vec record }
    input_record_type = Types.Record(
        {
            "id": Types.Nat,
            "name": Types.Text,
            "active": Types.Bool,
            "metadata": Types.Opt(metadata_type),
            "items": Types.Vec(item_type),
        }
    )

    # Build input value with all types and double nesting
    input_value = {
        "id": 1,
        "name": "Test Complex",
        "active": True,
        "metadata": [
            {
                "tags": ["tag1", "tag2", "tag3"],
                "status": {"Active": None},
                "details": {"count": 5, "items": [10, 20, 30, 40, 50]},
            }
        ],  # Opt: [value] means Some(value)
        "items": [
            {"value": "item1", "score": 100},
            {"value": "item2", "score": 200},
        ],
    }

    # Encode vec of records
    params = [{"type": Types.Vec(input_record_type), "value": [input_value]}]

    payload = encode(params)
    response_bytes = pic.update_call(principal_id, "complex_test", payload)
    decoded = decode_candid_text(response_bytes)
    print(f"complex_test([complex_record]) -> {decoded!r}")
    # Should return vec of Result variants (Ok/Err)
    assert response_bytes is not None
    assert len(response_bytes) > 0
    print("✅ Complex test passed: all types with double nesting and vec")


# Backward compatibility: Run all tests if executed directly
def run_all_tests() -> None:
    """Run all test cases (backward compatibility for direct execution)"""
    print("=" * 60)
    print("Running types_example test suite")
    print("=" * 60)

    # Use pytest to run all tests
    pytest.main([__file__, "-v"])


if __name__ == "__main__":
    run_all_tests()
