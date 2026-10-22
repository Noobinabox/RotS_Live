"""Each scenario's recorded run time, kept in build/integration/durations.json so the run plan can estimate a run before it starts."""

from __future__ import annotations

import json
import math
import os
import tempfile
from pathlib import Path
from typing import Mapping

# The durations file, relative to the repository root: a JSON object of node id -> seconds.
DURATIONS_RELATIVE = Path("build") / "integration" / "durations.json"
# The marker of the tests whose times are recorded; the harness's own unit tests are not.
SCENARIO_MARKER = "scenario"
# The merged file's permissions; mkstemp's own let only the owner read it.
DURATIONS_FILE_MODE = 0o644


def read_durations(path: Path) -> dict[str, float]:
    """The seconds recorded for each node id in path.

    A missing or unreadable file, or one that is not a JSON object, reads as empty; an entry whose
    value is not a finite number of seconds of at least 0 is left out.
    """
    try:
        content = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    if not isinstance(content, dict):
        return {}
    recorded = {}
    for node_id, seconds in content.items():
        # bool is an int to Python, but true is no number of seconds.
        if isinstance(seconds, (int, float)) and not isinstance(seconds, bool) and math.isfinite(seconds) and seconds >= 0:
            recorded[node_id] = float(seconds)
    return recorded


def merge_durations(path: Path, recorded: Mapping[str, float]) -> None:
    """Replaces path's entries for the recorded node ids, keeping every other entry; raises OSError.

    The merged file is written beside path and then moved over it, so an interrupted write never
    leaves a broken file. Two runs finishing together can each miss the other's entries; the
    next run records them again.
    """
    merged = read_durations(path)
    merged.update(recorded)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as temporary:
            json.dump(merged, temporary, indent=1, sort_keys=True)
        os.chmod(temporary_name, DURATIONS_FILE_MODE)
        os.replace(temporary_name, path)
    except BaseException:
        Path(temporary_name).unlink(missing_ok=True)
        raise


class DurationRecorder:
    """A pytest plugin adding up each scenario's setup, call and teardown seconds by node id.

    Registered in the process the run started in, which also receives the reports of pytest-xdist
    workers.
    """

    def __init__(self) -> None:
        # Seconds per node id of every test that reported a phase in this run.
        self.seconds: dict[str, float] = {}

    def pytest_runtest_logreport(self, report) -> None:
        """Adds the reported phase's seconds to its test's total when the test is marked scenario."""
        if SCENARIO_MARKER not in report.keywords:
            return
        self.seconds[report.nodeid] = self.seconds.get(report.nodeid, 0.0) + report.duration
