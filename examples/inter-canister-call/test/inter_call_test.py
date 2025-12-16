#!/usr/bin/env python3
"""
Reusable helpers for testing example canisters with PocketIC.

Intended usage:
  - import this module from your own test code
  - call `run_single_query_test(...)` or use the lower-level helpers
"""

from __future__ import annotations

from typing import Optional

from test_support_pocketic import (
    install_example_canister,
    query_and_expect_substr,
)


def run_single_query_test(
    example: str,
    method: str,
    expected_substr: Optional[str] = None,
    *,
    auto_build: bool = True,
) -> str:
    """
    Convenience helper:
      1. (Optionally) build the given example via build.py
      2. Install it into PocketIC
      3. Perform a no-arg query call to `method`
      4. Optionally assert that `expected_substr` is contained in the decoded text

    Returns the decoded text response.
    """
    print(
        f"[run_single_query_test] example={example!r}, "
        f"method={method!r}, expected_substr={expected_substr!r}, "
        f"auto_build={auto_build}"
    )

    pic, canister_id = install_example_canister(
        example,
        auto_build=auto_build,
    )
    decoded = query_and_expect_substr(
        pic,
        canister_id,
        method,
        expected_substr=expected_substr,
    )
    print(f"[run_single_query_test] decoded response: {decoded!r}")
    return decoded

