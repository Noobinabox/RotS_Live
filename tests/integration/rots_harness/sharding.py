"""Splits a run's collected tests into numbered shards, so parallel jobs each run one share."""

from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Iterable

# A shard value: the shard's number, a slash and the shard count, digits only.
SHARD_PATTERN = re.compile(r"([0-9]+)/([0-9]+)")


@dataclass(frozen=True)
class Shard:
    """One share of a run split into count shards; number is 1-based."""

    # Which share this run keeps, from 1 to count.
    number: int
    # How many shares the run is split into.
    count: int


def parse_shard(value: str) -> Shard:
    """The shard a 'k/N' value names, such as '2/4'; raises ValueError unless 1 <= k <= N."""
    match = SHARD_PATTERN.fullmatch(value)
    if match is None:
        raise ValueError(f"expected k/N with whole numbers, such as 2/4, not {value!r}")
    number = int(match.group(1))
    count = int(match.group(2))
    if count < 1 or number < 1 or number > count:
        raise ValueError(f"expected k/N with 1 <= k <= N, not {value!r}")
    return Shard(number, count)


def in_shard(node_ids: Iterable[str], shard: Shard) -> set[str]:
    """The node ids the shard keeps: every count-th id in sorted order, starting at the shard's number.

    Sorting first makes the split independent of collection order, so every shard of the same
    selection agrees on which shard owns each test.
    """
    ordered = sorted(set(node_ids))
    return {node_id for index, node_id in enumerate(ordered) if index % shard.count == shard.number - 1}
