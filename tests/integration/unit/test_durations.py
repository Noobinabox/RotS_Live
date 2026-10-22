from __future__ import annotations

import json
import os
import stat
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from rots_harness import durations


@dataclass
class FakeReport:
    """The parts of a pytest TestReport the recorder reads."""

    # The test's node id.
    nodeid: str
    # The phase: setup, call or teardown.
    when: str
    # Seconds the phase took.
    duration: float
    # The test's keywords, its marker names among them.
    keywords: dict = field(default_factory=lambda: {"scenario": 1})


def test_a_missing_file_reads_as_empty(tmp_path: Path) -> None:
    assert durations.read_durations(tmp_path / "durations.json") == {}


@pytest.mark.parametrize("content", ["{not json", "[1, 2]", '"text"'])
def test_an_unreadable_file_reads_as_empty(tmp_path: Path, content: str) -> None:
    path = tmp_path / "durations.json"
    path.write_text(content, encoding="utf-8")
    assert durations.read_durations(path) == {}


def test_entries_that_are_not_a_number_of_seconds_are_ignored(tmp_path: Path) -> None:
    path = tmp_path / "durations.json"
    path.write_text(json.dumps({"good": 3.5, "whole": 2, "text": "slow", "flag": True, "negative": -1}), encoding="utf-8")
    assert durations.read_durations(path) == {"good": 3.5, "whole": 2.0}


def test_merge_keeps_untouched_entries_and_replaces_recorded_ones(tmp_path: Path) -> None:
    path = tmp_path / "durations.json"
    path.write_text(json.dumps({"kept": 4.0, "rerun": 9.0}), encoding="utf-8")
    durations.merge_durations(path, {"rerun": 2.5, "new": 1.0})
    assert durations.read_durations(path) == {"kept": 4.0, "rerun": 2.5, "new": 1.0}


def test_merge_over_an_unreadable_file_starts_afresh(tmp_path: Path) -> None:
    path = tmp_path / "durations.json"
    path.write_text("{broken", encoding="utf-8")
    durations.merge_durations(path, {"new": 1.0})
    assert durations.read_durations(path) == {"new": 1.0}


def test_merge_creates_the_folder(tmp_path: Path) -> None:
    path = tmp_path / "build" / "integration" / "durations.json"
    durations.merge_durations(path, {"new": 1.0})
    assert durations.read_durations(path) == {"new": 1.0}


def test_a_failed_replace_leaves_the_old_file_and_no_temporary_file(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    path = tmp_path / "durations.json"
    path.write_text(json.dumps({"kept": 4.0}), encoding="utf-8")

    def failing_replace(source, destination) -> None:
        raise OSError("disk full")

    monkeypatch.setattr(os, "replace", failing_replace)
    with pytest.raises(OSError):
        durations.merge_durations(path, {"new": 1.0})
    assert durations.read_durations(path) == {"kept": 4.0}
    assert [entry.name for entry in tmp_path.iterdir()] == ["durations.json"]


def test_the_recorder_adds_setup_call_and_teardown_per_test() -> None:
    recorder = durations.DurationRecorder()
    for report in [
        FakeReport("first", "setup", 1.0),
        FakeReport("first", "call", 2.5),
        FakeReport("second", "setup", 0.5),
        FakeReport("first", "teardown", 0.5),
    ]:
        recorder.pytest_runtest_logreport(report)
    assert recorder.seconds == {"first": 4.0, "second": 0.5}


def test_the_recorder_ignores_tests_that_are_not_scenarios() -> None:
    recorder = durations.DurationRecorder()
    recorder.pytest_runtest_logreport(FakeReport("unit/test_x.py::test_y", "call", 0.1, keywords={"test_y": 1}))
    assert recorder.seconds == {}


def test_the_merged_file_is_readable_by_others(tmp_path: Path) -> None:
    path = tmp_path / "durations.json"
    durations.merge_durations(path, {"new": 1.0})
    assert path.stat().st_mode & stat.S_IROTH
