"""Chunk helpers shared by tests and upload logic."""

from __future__ import annotations

DEFAULT_CHUNK_SIZE = 1_900_000  # conservative update-safe chunk size (~1.9MB)



def chunk_sizes(total_bytes: int, chunk_size: int = DEFAULT_CHUNK_SIZE) -> list[int]:
    """Return the sizes of fragments required to cover *total_bytes*."""
    if total_bytes < 0:
        raise ValueError("total_bytes must be non-negative")
    sizes: list[int] = []
    offset = 0
    while offset < total_bytes:
        remaining = total_bytes - offset
        sizes.append(chunk_size if remaining >= chunk_size else remaining)
        offset += chunk_size
    return sizes


def max_chunk_size(total_bytes: int, chunk_size: int = DEFAULT_CHUNK_SIZE) -> int:
    """Return the size of the largest chunk needed for the given payload."""
    sizes = chunk_sizes(total_bytes, chunk_size)
    return max(sizes) if sizes else 0
