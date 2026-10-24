from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import pytest

# The integration conftest.py, which pytest has already loaded as the module conftest.
import conftest
from rots_harness.characters import IMPLEMENTOR, CharacterSet
from rots_harness.timecontrol import REAL_TIME_SCHEDULE

# An entry token as the harness binary's ready line gives it.
ENTRY_TOKEN = "54a5c0ffee0123456789abcdef012345"


@dataclass(frozen=True)
class FakeMarker:
    """A marker as get_closest_marker returns it; schedule_for reads only its arguments."""

    # The marker's positional arguments.
    args: tuple


class FakeNode:
    """A collected test carrying the given markers by name."""

    def __init__(self, markers: dict[str, FakeMarker]) -> None:
        # The test's markers by name.
        self._markers = markers
        # The test's node id, which a failure names.
        self.nodeid = "test_fake.py::test_fake"

    def get_closest_marker(self, name: str) -> Optional[FakeMarker]:
        return self._markers.get(name)


@dataclass
class FakeRequest:
    """A fixture request whose node carries the given markers."""

    # The test being set up.
    node: FakeNode


def schedule_for_markers(**markers: tuple) -> Optional[str]:
    """What conftest.schedule_for gives a test carrying markers, each given by name with its arguments."""
    request = FakeRequest(FakeNode({name: FakeMarker(args) for name, args in markers.items()}))
    return conftest.schedule_for(request)  # type: ignore[arg-type]


def test_a_test_without_markers_runs_the_harness_binary_with_the_real_time_schedule() -> None:
    assert schedule_for_markers() == REAL_TIME_SCHEDULE


def test_a_real_login_test_runs_plain_ageland() -> None:
    assert schedule_for_markers(real_login=()) is None


def test_a_schedule_marker_names_the_harness_schedule() -> None:
    assert schedule_for_markers(schedule=("affect-hours",)) == "affect-hours"


def test_a_test_with_both_markers_fails_naming_real_login() -> None:
    with pytest.raises(pytest.fail.Exception, match="real_login"):
        schedule_for_markers(schedule=("affect-hours",), real_login=())


class RecordingSession:
    """Stands in for GameSession: records its constructor's entry token and whether login() ran."""

    # Every session constructed, in order.
    created: list["RecordingSession"] = []

    def __init__(self, handle, character_name: str, email: str, password: str, transcript_dir: Path, entry_token: Optional[str] = None) -> None:
        # The character the session plays.
        self.character_name = character_name
        # The entry token HarnessServer.login handed over.
        self.entry_token = entry_token
        # Whether login() was called.
        self.logged_in = False
        RecordingSession.created.append(self)

    def login(self) -> None:
        self.logged_in = True

    def close(self) -> None:
        pass


def harness_server(entry_token: Optional[str], schedule: Optional[str], time_control: object = None) -> conftest.HarnessServer:
    """A HarnessServer for the implementor with no running server behind it."""
    return conftest.HarnessServer(
        handle=None,  # type: ignore[arg-type]
        lib_dir=Path("lib"),
        run_dir=Path("run"),
        character_set=CharacterSet((IMPLEMENTOR,)),
        monitor=None,  # type: ignore[arg-type]
        time_control=time_control,  # type: ignore[arg-type]
        schedule=schedule,
        entry_token=entry_token,
    )


@pytest.mark.parametrize("entry_token", [ENTRY_TOKEN, None])
def test_login_hands_the_servers_entry_token_to_the_session(monkeypatch: pytest.MonkeyPatch, entry_token: Optional[str]) -> None:
    monkeypatch.setattr(conftest, "GameSession", RecordingSession)
    monkeypatch.setattr(RecordingSession, "created", [])
    server = harness_server(entry_token, REAL_TIME_SCHEDULE if entry_token else None)
    game_session = server.login("Testimm")
    assert [created.entry_token for created in RecordingSession.created] == [entry_token]
    assert game_session.logged_in
    assert server.sessions == [game_session]


def test_advance_hours_on_a_real_time_server_fails_naming_the_schedule_marker() -> None:
    server = harness_server(ENTRY_TOKEN, REAL_TIME_SCHEDULE, time_control=object())
    with pytest.raises(pytest.fail.Exception, match=r"needs @pytest\.mark\.schedule"):
        server.advance_hours(1)


def test_advance_hours_on_plain_ageland_fails_naming_the_schedule_marker() -> None:
    server = harness_server(None, None)
    with pytest.raises(pytest.fail.Exception, match=r"needs @pytest\.mark\.schedule"):
        server.advance_hours(1)
