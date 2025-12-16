#!/usr/bin/env python3
"""
Example of using the PocketIC helpers to call an UPDATE method with arguments.

This test targets the `trigger_call` update method which now has the simple
signature:

  (text) -> (text)

The single text argument is interpreted by the canister as either:
  "<method_name>" or "<ignored_prefix>,<method_name>".

For this example we just send "<method_name>".
"""

from ic.candid import Types, encode
from ic.principal import Principal
from test_support_pocketic import (
    decode_candid_text,
    install_example_canister,
)


def test_trigger() -> None:
    """End-to-end test for `trigger_call(text) -> (text)`."""
    print("=== test_trigger: start ===")
    example = "inter-canister-call"

    # 1. 构建并安装 example canister
    pic, canister_id = install_example_canister(example, auto_build=True)

    # 2. 准备 Candid 参数：trigger_call(text)
    target_method = "increment"
    # target_method = "greet_no_arg"
    arg_text = target_method  # 也可以用 "<prefix>,greet_no_arg"
    params = [{"type": Types.Text, "value": arg_text}]
    payload = encode(params)

    # 3. 调用 trigger_call（update）
    print(
        f"[test_trigger] calling trigger_call on {canister_id} "
        f"with arg_text={arg_text!r}"
    )
    # PocketIC's create_and_install_canister_with_candid already returns a
    # Principal-like canister_id, so we can pass it directly here.
    response_bytes = pic.update_call(canister_id, "trigger_call", payload)

    # 4. 打印结果
    decoded = decode_candid_text(response_bytes)
    print(f"=== test_trigger: done, decoded={decoded!r} ===")


if __name__ == "__main__":
    test_trigger()
