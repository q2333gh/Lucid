#!/usr/bin/env python3
"""
Helper to encode a text prompt with tiktoken, call the C inference binary, and decode output tokens.
"""
import argparse
import subprocess
import sys
from pathlib import Path

try:
    import tiktoken
except ImportError as e:
    sys.stderr.write("tiktoken is required (install in your venv)\n")
    raise


def parse_args():
    ap = argparse.ArgumentParser(
        description="Run GPT-2 C inference with a text prompt."
    )
    ap.add_argument("--prompt", required=True, help="Input text prompt")
    ap.add_argument(
        "--max-new", type=int, default=32, help="Max new tokens to generate"
    )
    ap.add_argument("--temp", type=float, default=1.0, help="Sampling temperature")
    ap.add_argument("--top-k", type=int, default=40, help="Top-k sampling cutoff")
    ap.add_argument(
        "--ckpt", default="gpt2_124M.bin", help="Checkpoint path for run_gpt2"
    )
    ap.add_argument(
        "--run-bin",
        default=str(Path(__file__).resolve().parent.parent / "run_gpt2.out"),
        help="Path to compiled run_gpt2 binary",
    )
    return ap.parse_args()


def encode(prompt: str):
    enc = tiktoken.get_encoding("gpt2")
    return enc.encode(prompt)


def decode(tokens):
    enc = tiktoken.get_encoding("gpt2")
    return enc.decode(tokens)


def call_run_gpt2(run_bin, token_ids, max_new, temp, top_k, ckpt):
    tokens_arg = " ".join(str(t) for t in token_ids)
    cmd = [
        str(run_bin),
        "--tokens",
        tokens_arg,
        "--max-new",
        str(max_new),
        "--temp",
        str(temp),
        "--top-k",
        str(top_k),
        "--ckpt",
        ckpt,
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    return result.stdout


def extract_generated_tokens(run_output: str):
    lines = [ln.strip() for ln in run_output.splitlines() if ln.strip()]
    # find the line after "Generated token ids:"
    for i, ln in enumerate(lines):
        if ln.startswith("Generated token ids:"):
            if i + 1 < len(lines):
                tok_line = lines[i + 1]
                return [int(x) for x in tok_line.split()]
    raise ValueError("Could not parse generated tokens from run_gpt2 output")


def main():
    args = parse_args()
    prompt_tokens = encode(args.prompt)
    run_bin = Path(args.run_bin)
    if not run_bin.exists():
        fallback = Path(__file__).resolve().parent.parent / "run_gpt2.out"
        if fallback.exists():
            run_bin = fallback
        else:
            sys.stderr.write(f"run_gpt2 binary not found at {run_bin}\n")
            raise SystemExit(1)
    run_out = call_run_gpt2(
        run_bin, prompt_tokens, args.max_new, args.temp, args.top_k, args.ckpt
    )
    try:
        gen_tokens = extract_generated_tokens(run_out)
    except ValueError as e:
        sys.stderr.write(run_out)
        raise

    text = decode(gen_tokens)
    print("Prompt tokens:", prompt_tokens)
    print("Generated tokens:", gen_tokens)
    print("Decoded text:\n", text)


if __name__ == "__main__":
    main()
