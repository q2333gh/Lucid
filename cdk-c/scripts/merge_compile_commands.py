#!/usr/bin/env python3
"""
Utility for merging multiple compile_commands.json files into a single database.

By default this script combines the WASI build database at cdk-c/build/compile_commands.json
with the host Criterion test database at cdk-c/test/build/compile_commands.json and writes
the merged result to the repo root compile_commands.json. Paths are configurable via
CLI arguments so the script can also be reused for other submodules.
"""
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Sequence


DEFAULT_INPUTS: Sequence[str] = (
    "cdk-c/build/compile_commands.json",
    "cdk-c/test/build/compile_commands.json",
)
DEFAULT_OUTPUT = "cdk-c/compile_commands.json"


@dataclass(frozen=True)
class EntryKey:
    file: str
    signature: str


def resolve_path(raw: str, root: Path) -> Path:
    path = Path(raw)
    return path if path.is_absolute() else root / path


def expand_candidate(path: Path) -> Path:
    if path.is_dir():
        return path / "compile_commands.json"
    return path


def read_entries(path: Path) -> List[dict]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, list):
        raise ValueError(f"{path} does not contain a JSON array")
    return data


def summarize_entry(entry: dict) -> EntryKey:
    file_path = entry.get("file")
    if not isinstance(file_path, str):
        raise ValueError("compile_commands entry is missing a string 'file' field")
    if "command" in entry and isinstance(entry["command"], str):
        signature = entry["command"]
    else:
        args = entry.get("arguments")
        if not isinstance(args, list):
            raise ValueError(
                f"entry for {file_path} is missing both 'command' and 'arguments'"
            )
        signature = " ".join(str(part) for part in args)
    return EntryKey(file=file_path, signature=signature)


def dedupe(entries: Iterable[dict]) -> List[dict]:
    seen = set()
    unique: List[dict] = []
    for entry in entries:
        key = summarize_entry(entry)
        if key in seen:
            continue
        seen.add(key)
        unique.append(entry)
    return unique


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(
        description=(
            "Merge multiple compile_commands.json files into a single database "
            "for clangd or other tooling."
        )
    )
    parser.add_argument(
        "--root",
        default=str(repo_root),
        help=f"Repository root (defaults to {repo_root})",
    )
    parser.add_argument(
        "--inputs",
        nargs="+",
        default=list(DEFAULT_INPUTS),
        help=(
            "List of compile_commands paths or directories to merge. "
            "Paths can be absolute or relative to --root."
        ),
    )
    parser.add_argument(
        "--output",
        default=DEFAULT_OUTPUT,
        help="Output path for the merged compile_commands.json (relative to --root unless absolute).",
    )
    parser.add_argument(
        "--keep-duplicates",
        action="store_true",
        help="Disable deduplication and keep all entries even if they share the same file+command signature.",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress informational logging; only errors will be printed.",
    )
    return parser.parse_args()


def log(message: str, quiet: bool) -> None:
    if not quiet:
        print(message)


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()
    output_path = resolve_path(args.output, root)

    merged: List[dict] = []
    discovered_inputs = 0
    for raw_input in args.inputs:
        candidate = expand_candidate(resolve_path(raw_input, root))
        if not candidate.exists():
            log(f"[skip] {candidate} does not exist", args.quiet)
            continue
        discovered_inputs += 1
        try:
            entries = read_entries(candidate)
        except Exception as exc:
            print(f"[error] failed to read {candidate}: {exc}", file=sys.stderr)
            return 1
        merged.extend(entries)
        log(f"[ok] loaded {len(entries)} entries from {candidate}", args.quiet)

    if discovered_inputs == 0:
        print(
            "[error] none of the requested inputs were found; "
            "double-check --root/--inputs",
            file=sys.stderr,
        )
        return 1

    if not args.keep_duplicates:
        before = len(merged)
        merged = dedupe(merged)
        log(f"[info] deduplicated {before} -> {len(merged)} entries", args.quiet)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as handle:
        json.dump(merged, handle, indent=2)
        handle.write("\n")
    log(f"[done] wrote {len(merged)} entries to {output_path}", args.quiet)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

