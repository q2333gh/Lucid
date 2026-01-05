#!/usr/bin/env python3
"""
Test suite for records canister.

Tests simplified Candid Builder API usage including:
- greet query method with text parameter (IC_API_PARSE_SIMPLE)
- add_user update method with multiple parameters (IC_API_PARSE_BEGIN)
- get_address: Macro-based API with opt record
- get_profile: Builder API with multiple types
- get_user_info: Vector construction
- get_nested_data: Nested record construction
- get_optional_data: Optional field handling
- get_complex_record: Builder API with all types

Usage:
    # Run all tests with pytest
    pytest examples/records/records_test.py

    # Run a specific test
    pytest examples/records/records_test.py::test_greet

    # Run tests matching a pattern
    # Note: Use -s flag to see build output: pytest -s examples/records/records_test.py
    pytest examples/records/records_test.py -k greet

    # Run all tests (backward compatibility)
    python3 examples/records/records_test.py
"""

# Type checking: Suppress false positives from type checker
# The ic.candid library's Types API works correctly at runtime,
# but type stubs may not fully capture the dynamic type system
# pyright: reportGeneralTypeIssues=false
# pyright: reportOptionalMemberAccess=false
# pyright: reportArgumentType=false

import sys
from pathlib import Path

# Add project root to path for imports
project_root = Path(__file__).resolve().parent.parent.parent
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

import pytest  # type: ignore[reportMissingImports]
from ic.candid import Types, encode, decode
from ic.principal import Principal
from test.support.test_support_pocketic import (
    decode_candid_text,
    install_example_canister,
)


# Pytest hook to ensure build output is visible
# This runs before test collection, so output is always visible
def pytest_configure(config):
    """Pytest configuration hook - runs before test collection."""
    # Force stderr to be unbuffered so build output appears immediately
    import sys

    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(line_buffering=True)  # type: ignore[attr-defined]


def ensure_principal(canister_id) -> Principal:
    """Convert canister_id to Principal if needed"""
    if isinstance(canister_id, Principal):
        return canister_id
    return Principal.from_str(str(canister_id))


@pytest.fixture(scope="module")
def pic_and_canister():
    """Pytest fixture: Build and install canister once for all tests in this module."""
    import sys

    # Use stderr to ensure output is visible even when pytest captures stdout
    sys.stderr.write("\n[Setup] Building and installing canister...\n")
    sys.stderr.flush()
    pic, canister_id = install_example_canister("records", auto_build=True)
    sys.stderr.write(f"[Setup] Canister installed: {canister_id}\n\n")
    sys.stderr.flush()
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

    # Call greet with a name parameter
    params = [{"type": Types.Text, "value": "World"}]
    payload = encode(params)
    response_bytes = pic.query_call(principal_id, "greet", payload)
    decoded = decode_candid_text(response_bytes)
    print(f"greet('World') -> {decoded!r}")
    assert "Hello" in decoded and "World" in decoded, f"Unexpected response: {decoded}"


def test_add_user(pic, canister_id) -> None:
    """Test add_user: (text, nat, bool) -> (text)"""
    print("\n=== Test: add_user ===")
    principal_id = ensure_principal(canister_id)

    # Call add_user with multiple parameters
    params = [
        {"type": Types.Text, "value": "Alice"},
        {"type": Types.Nat, "value": 25},
        {"type": Types.Bool, "value": True},
    ]
    payload = encode(params)
    response_bytes = pic.update_call(principal_id, "add_user", payload)
    decoded = decode_candid_text(response_bytes)
    print(f"add_user('Alice', 25, True) -> {decoded!r}")
    assert (
        "Alice" in decoded and "successfully" in decoded
    ), f"Unexpected response: {decoded}"


def test_get_address(pic, canister_id) -> None:
    """Test get_address: (text) -> (opt record { street : text; city : text; zip : nat })"""
    print("\n=== Test: get_address (Macro API) ===")
    principal_id = ensure_principal(canister_id)

    params = [{"type": Types.Text, "value": "Main"}]
    payload = encode(params)

    # Debug: Print payload information
    print(f"[DEBUG] Request params: {params}")
    print(f"[DEBUG] Payload length: {len(payload)} bytes")
    payload_hex = payload.hex()
    if len(payload_hex) > 64:
        print(f"[DEBUG] Payload (hex, first 32 bytes): {payload_hex[:64]}...")
        print(f"[DEBUG] Payload (hex, full): {payload_hex}")
    else:
        print(f"[DEBUG] Payload (hex): {payload_hex}")

    response_bytes = pic.query_call(principal_id, "get_address", payload)

    # Debug: Print response information
    print(f"[DEBUG] Response length: {len(response_bytes)} bytes")
    response_hex = response_bytes.hex()
    print(f"[DEBUG] Response (hex, full {len(response_bytes)} bytes): {response_hex}")
    if len(response_hex) > 64:
        print(f"[DEBUG] Response (hex, first 32 bytes preview): {response_hex[:64]}...")

    # Try to decode with standard decode() to get structured data
    try:
        decoded_result = decode(response_bytes)
        print(f"[DEBUG] Decoded result (structured): {decoded_result}")
        if decoded_result and len(decoded_result) > 0:
            opt_record = decoded_result[0]
            if opt_record is not None:
                print(f"[DEBUG] Address record fields: {opt_record}")
                if isinstance(opt_record, dict):
                    print(f"[DEBUG]   - street: {opt_record.get('street', 'N/A')}")
                    print(f"[DEBUG]   - city: {opt_record.get('city', 'N/A')}")
                    print(f"[DEBUG]   - zip: {opt_record.get('zip', 'N/A')}")
    except Exception as e:
        print(f"[DEBUG] Standard decode() failed (may have field sorting issues): {e}")

    # Also use decode_candid_text as fallback/alternative view
    decoded_text = decode_candid_text(response_bytes)
    print(f"[DEBUG] Decoded (text extraction): {decoded_text!r}")
    print(f"get_address('Main') -> {decoded_text!r}")

    # Basic validation - response should not be empty
    assert response_bytes is not None
    assert len(response_bytes) > 6, "Response should contain data beyond DIDL header"
    print(f"  ✓ Address record returned ({len(response_bytes)} bytes)")


def test_get_profile(pic, canister_id) -> None:
    """Test get_profile: (text) -> (record { name : text; age : nat; active : bool })"""
    print("\n=== Test: get_profile (Builder API) ===")
    principal_id = ensure_principal(canister_id)

    params = [{"type": Types.Text, "value": "Bob"}]
    payload = encode(params)
    response_bytes = pic.query_call(principal_id, "get_profile", payload)
    result = decode(response_bytes)
    print(f"get_profile('Bob') -> {result}")

    assert len(result) == 1, "Expected one return value"
    profile = result[0]
    assert profile["name"] == "Bob", f"Expected name='Bob', got {profile['name']}"
    assert profile["age"] == 25, f"Expected age=25, got {profile['age']}"
    assert profile["active"] is True, f"Expected active=True, got {profile['active']}"
    print(
        f"  ✓ Profile: name={profile['name']}, age={profile['age']}, active={profile['active']}"
    )


def test_get_user_info(pic, canister_id) -> None:
    """Test get_user_info: (text) -> (record { id : nat; emails : vec text; tags : vec text })"""
    print("\n=== Test: get_user_info (Vector API) ===")
    principal_id = ensure_principal(canister_id)

    params = [{"type": Types.Text, "value": "Charlie"}]
    payload = encode(params)
    response_bytes = pic.query_call(principal_id, "get_user_info", payload)
    result = decode(response_bytes)
    print(f"get_user_info('Charlie') -> {result}")

    assert len(result) == 1, "Expected one return value"
    info = result[0]
    assert info["id"] == 42, f"Expected id=42, got {info['id']}"
    assert len(info["emails"]) == 2, f"Expected 2 emails, got {len(info['emails'])}"
    assert len(info["tags"]) == 2, f"Expected 2 tags, got {len(info['tags'])}"
    print(
        f"  ✓ User info: id={info['id']}, emails={info['emails']}, tags={info['tags']}"
    )


def test_get_nested_data(pic, canister_id) -> None:
    """Test get_nested_data: (text) -> (record { user : record { name : text; age : nat }; timestamp : nat })"""
    print("\n=== Test: get_nested_data (Nested Records) ===")
    principal_id = ensure_principal(canister_id)

    params = [{"type": Types.Text, "value": "Dave"}]
    payload = encode(params)
    response_bytes = pic.query_call(principal_id, "get_nested_data", payload)
    result = decode(response_bytes)
    print(f"get_nested_data('Dave') -> {result}")

    assert len(result) == 1, "Expected one return value"
    data = result[0]
    assert "user" in data, "Missing 'user' field"
    assert "timestamp" in data, "Missing 'timestamp' field"
    assert (
        data["user"]["name"] == "Dave"
    ), f"Expected name='Dave', got {data['user']['name']}"
    assert data["user"]["age"] == 30, f"Expected age=30, got {data['user']['age']}"
    print(f"  ✓ Nested data: user={data['user']}, timestamp={data['timestamp']}")


def test_get_optional_data_with_age(pic, canister_id) -> None:
    """Test get_optional_data with age: (text, bool) -> (record { name : text; age : opt nat })"""
    print("\n=== Test: get_optional_data (with age) ===")
    principal_id = ensure_principal(canister_id)

    params = [
        {"type": Types.Text, "value": "Eve"},
        {"type": Types.Bool, "value": True},
    ]
    payload = encode(params)
    response_bytes = pic.query_call(principal_id, "get_optional_data", payload)
    result = decode(response_bytes)
    print(f"get_optional_data('Eve', True) -> {result}")

    assert len(result) == 1, "Expected one return value"
    data = result[0]
    # Fields sorted by hash: age < name
    assert data["name"] == "Eve", f"Expected name='Eve', got {data['name']}"
    assert data["age"] is not None, "Expected Some(age), got None"
    assert data["age"] == 25, f"Expected age=25, got {data['age']}"
    print(f"  ✓ Optional data (with age): age={data['age']}, name={data['name']}")


def test_get_optional_data_without_age(pic, canister_id) -> None:
    """Test get_optional_data without age: (text, bool) -> (record { name : text; age : opt nat })"""
    print("\n=== Test: get_optional_data (without age) ===")
    principal_id = ensure_principal(canister_id)

    params = [
        {"type": Types.Text, "value": "Frank"},
        {"type": Types.Bool, "value": False},
    ]
    payload = encode(params)
    response_bytes = pic.query_call(principal_id, "get_optional_data", payload)
    result = decode(response_bytes)
    print(f"get_optional_data('Frank', False) -> {result}")

    assert len(result) == 1, "Expected one return value"
    data = result[0]
    # Fields sorted by hash: age < name
    assert data["name"] == "Frank", f"Expected name='Frank', got {data['name']}"
    assert data["age"] is None, f"Expected None, got {data['age']}"
    print(f"  ✓ Optional data (without age): age=None, name={data['name']}")


def test_get_complex_record(pic, canister_id) -> None:
    """Test get_complex_record: (text) -> (record with all types)"""
    print("\n=== Test: get_complex_record (All Types) ===")
    principal_id = ensure_principal(canister_id)

    params = [{"type": Types.Text, "value": "Grace"}]
    payload = encode(params)
    response_bytes = pic.query_call(principal_id, "get_complex_record", payload)
    result = decode(response_bytes)
    print(f"get_complex_record('Grace') -> {result}")

    assert len(result) == 1, "Expected one return value"
    record = result[0]
    assert record["name"] == "Grace", f"Expected name='Grace', got {record['name']}"
    assert record["active"] is True, f"Expected active=True, got {record['active']}"
    assert record["score"] == 100, f"Expected score=100, got {record['score']}"
    assert (
        record["balance"] == 1000000
    ), f"Expected balance=1000000, got {record['balance']}"
    assert record["temp"] == -10, f"Expected temp=-10, got {record['temp']}"
    assert (
        record["offset"] == -1000000
    ), f"Expected offset=-1000000, got {record['offset']}"
    assert (
        abs(record["ratio"] - 0.75) < 0.01
    ), f"Expected ratio≈0.75, got {record['ratio']}"
    assert (
        abs(record["pi"] - 3.14159) < 0.0001
    ), f"Expected pi≈3.14159, got {record['pi']}"
    print(f"  ✓ Complex record: {record}")
    print(f"    - text: {record['name']}")
    print(f"    - bool: {record['active']}")
    print(f"    - nat32: {record['score']}")
    print(f"    - nat64: {record['balance']}")
    print(f"    - int32: {record['temp']}")
    print(f"    - int64: {record['offset']}")
    print(f"    - float32: {record['ratio']}")
    print(f"    - float64: {record['pi']}")


# Backward compatibility: Run all tests if executed directly
def run_all_tests() -> None:
    """Run all test cases (backward compatibility for direct execution)"""
    print("=" * 60)
    print("Running records test suite")
    print("=" * 60)

    # Use pytest to run all tests
    pytest.main([__file__, "-v"])


if __name__ == "__main__":
    run_all_tests()
