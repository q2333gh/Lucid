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
from test_support_pocketic import decode_candid_text, install_multiple_examples


def test_trigger() -> None:
    """End-to-end test for `trigger_call(text) -> (text)`."""
    print("=== test_trigger: start ===")
    # 1. 在同一个 PocketIC 实例中安装两个 canister：adder 和 inter-canister-call
    pic, ids = install_multiple_examples(
        ["adder", "inter-canister-call"], auto_build=True
    )
    adder_id = ids["adder"]
    caller_id = ids["inter-canister-call"]

    # 2. 准备 Candid 参数：trigger_call(text)
    target_method = "increment"
    # 传给 trigger_call 的约定格式: "<callee>,<method_name>"
    # 这里使用 adder canister 的 principal 文本 + 方法名。
    from ic.principal import Principal  # local import to avoid circular issues

    if isinstance(adder_id, Principal):
        callee_text = adder_id.to_str()
    else:
        callee_text = str(adder_id)

    arg_text = f"{callee_text},{target_method}"
    params = [{"type": Types.Text, "value": arg_text}]
    payload = encode(params)

    # 3. 调用 trigger_call（update）
    print(
        f"[test_trigger] calling trigger_call on {caller_id} "
        f"with arg_text={arg_text!r} (adder_id={adder_id})"
    )
    response_bytes = pic.update_call(caller_id, "trigger_call", payload)

    # 4. 打印结果
    decoded = decode_candid_text(response_bytes)
    print(f"=== test_trigger: done, decoded={decoded!r} ===")


if __name__ == "__main__":
    test_trigger()
