from __future__ import annotations

import random

import pytest

from rots_harness.sharding import Shard, in_shard, parse_shard

# Node ids shaped like the scenarios', in no particular order.
NODE_IDS = [f"scenarios/test_{module}.py::test_case_{case}" for module in ("summon", "boot", "poison") for case in range(7)]


def test_a_shard_value_names_its_number_and_count() -> None:
    assert parse_shard("2/4") == Shard(number=2, count=4)


def test_a_single_shard_is_allowed() -> None:
    assert parse_shard("1/1") == Shard(number=1, count=1)


@pytest.mark.parametrize("value", ["", "2", "2/", "/4", "a/4", "2/b", "2/4/1", " 2/4", "2/4 ", "+2/4", "-1/4", "2.0/4", "0/4", "5/4", "1/0", "0/0"])
def test_a_malformed_shard_value_is_refused(value: str) -> None:
    with pytest.raises(ValueError):
        parse_shard(value)


@pytest.mark.parametrize("count", [1, 2, 3, 4, 5, 21, 40])
def test_every_test_lands_in_exactly_one_shard(count: int) -> None:
    shares = [in_shard(NODE_IDS, Shard(number, count)) for number in range(1, count + 1)]
    assert sorted(node_id for share in shares for node_id in share) == sorted(NODE_IDS)


def test_the_shares_differ_in_size_by_at_most_one() -> None:
    sizes = [len(in_shard(NODE_IDS, Shard(number, 4))) for number in range(1, 5)]
    assert max(sizes) - min(sizes) <= 1


def test_the_split_does_not_depend_on_collection_order() -> None:
    shuffled = list(NODE_IDS)
    random.Random(7).shuffle(shuffled)
    for number in range(1, 5):
        assert in_shard(shuffled, Shard(number, 4)) == in_shard(NODE_IDS, Shard(number, 4))


def test_the_shards_take_turns_in_sorted_order() -> None:
    ids = ["d", "b", "a", "c", "e"]
    assert in_shard(ids, Shard(1, 2)) == {"a", "c", "e"}
    assert in_shard(ids, Shard(2, 2)) == {"b", "d"}


def test_more_shards_than_tests_leaves_the_later_shards_empty() -> None:
    assert in_shard(["a", "b"], Shard(3, 4)) == set()
    assert in_shard(["a", "b"], Shard(4, 4)) == set()


def test_no_tests_gives_an_empty_share() -> None:
    assert in_shard([], Shard(1, 4)) == set()
