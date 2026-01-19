"""Py tests that drive chunk sizing logic before implementation (TDD style)."""

from __future__ import annotations

from tests.utils.chunk_utils import DEFAULT_CHUNK_SIZE, chunk_sizes, max_chunk_size


def test_chunk_sizes_zero() -> None:
    assert chunk_sizes(0) == []


def test_chunk_exact_boundary() -> None:
    total = DEFAULT_CHUNK_SIZE * 2
    sizes = chunk_sizes(total)
    assert sizes == [DEFAULT_CHUNK_SIZE, DEFAULT_CHUNK_SIZE]
    assert max_chunk_size(total) == DEFAULT_CHUNK_SIZE


def test_chunk_over_boundary() -> None:
    total = DEFAULT_CHUNK_SIZE * 2 + 123
    sizes = chunk_sizes(total)
    assert sizes[-1] == 123
    assert len(sizes) == 3
    assert max_chunk_size(total) == DEFAULT_CHUNK_SIZE


def test_chunk_just_one_byte() -> None:
    sizes = chunk_sizes(1)
    assert sizes == [1]
    assert max_chunk_size(1) == 1


def test_chunk_negative_total_raises() -> None:
    try:
        chunk_sizes(-1)
    except ValueError:
        return
    raise AssertionError("chunk_sizes should raise ValueError for negative input")
