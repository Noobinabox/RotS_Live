"""Disabled shaping commands each refuse, quoting what was typed, and shape master_mobile without
a number shows its usage."""

from __future__ import annotations

from typing import NamedTuple

import pytest

from rots_harness.characters import IMPLEMENTOR
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.shaping]


class Editor(NamedTuple):
    """An editor opened on a test-world record, and what it prints as it opens and closes."""

    open_line: str  # starts shaping the record
    prompt: str  # the prompt while editing (prompt_text in consts.cpp), first shown once the record loads
    release_marker: str  # what /free prints


# The mob, object, room and script editors on the test world's first record of each kind.
MOB_EDITOR = Editor("shape mob 1130", "Mobile: 1130>", "You released the mobile and stopped shaping.")
OBJECT_EDITOR = Editor("shape object 1130", "Object: 1130>", "You released an object and stopped shaping.")
ROOM_EDITOR = Editor("shape room 1101", "Shaping: 1101>", "You released the room and stopped shaping.")
SCRIPT_EDITOR = Editor("shape script 1100", "Script: 1100>", "You released the script and stopped shaping.")

# What a loader prints when the record or its file is missing (load_proto, load_object,
# load_room and load_script in the shape*.cpp files).
LOAD_FAILURE_MARKERS = ["could not open that file.", "No such mob or file corrupted.", "no object here.", "no room here."]
# Seconds to keep reading after a marker, for the prompt that follows it.
SETTLE_SECONDS = 0.5


def refusal(typed: str) -> str:
    """What shape_disabled (shapemob.cpp) prints for a command typed as typed."""
    return f'"{typed}" has been disabled due to a bug.'


def lines_of(text: str) -> list:
    """The text's lines without the carriage returns the game sends with each line end."""
    return [line.strip("\r") for line in text.split("\n")]


def reply_to(session: GameSession, line: str, outcomes: list) -> str:
    """Sends a line and returns what came back, up to and just after the first outcome."""
    session.send_line(line)
    return session.expect(outcomes) + session.drain(SETTLE_SECONDS)


# The disabled commands typed at the game prompt; the reason for each is beside its
# shape_disabled call (do_shape, shapemob.cpp).
COMMAND_LINE_CASES = ["shape mob new", "shape object new", "shape room new", "shape script new", "shape recalc_mobile"]

# The disabled commands typed inside an editor; the reason for each is beside its shape_disabled
# call in that editor's extra_coms function (the script editor's /12 is in shape_center_script).
# Only the room editor refuses a bare /add; the others refuse /add with a file name.
EDITOR_CASES = [
    (MOB_EDITOR, "/new"),
    (MOB_EDITOR, "/delete"),
    (MOB_EDITOR, "/add x"),
    (OBJECT_EDITOR, "/new"),
    (OBJECT_EDITOR, "/delete"),
    (OBJECT_EDITOR, "/add x"),
    (ROOM_EDITOR, "/new"),
    (ROOM_EDITOR, "/add"),
    (ROOM_EDITOR, "/delete"),
    (SCRIPT_EDITOR, "/new"),
    (SCRIPT_EDITOR, "/delete"),
    (SCRIPT_EDITOR, "/add x"),
    (SCRIPT_EDITOR, "/12"),
]


@pytest.mark.parametrize("typed", COMMAND_LINE_CASES)
@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_a_disabled_shape_command_refuses_with_what_was_typed(server, typed: str) -> None:
    """Pins #318: a disabled `shape` command starts no editor and says it is disabled, quoting
    the command as typed."""
    implementor = server.login(IMPLEMENTOR.name)

    reply = implementor.command(typed)

    assert refusal(typed) in lines_of(reply.text), reply.text
    assert "You start shaping" not in reply.text, reply.text


@pytest.mark.parametrize(
    ("editor", "typed"),
    EDITOR_CASES,
    ids=[f"{editor.open_line.split()[1]}-{typed}" for editor, typed in EDITOR_CASES],
)
@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_a_disabled_editor_command_refuses_with_what_was_typed(server, editor: Editor, typed: str) -> None:
    """Pins #318: inside an editor, a disabled command says it is disabled, quoting the command
    as typed, and the editor stays open on its record."""
    implementor = server.login(IMPLEMENTOR.name)

    opened = reply_to(implementor, editor.open_line, [editor.prompt, *LOAD_FAILURE_MARKERS])
    assert editor.prompt in opened, opened
    reply = reply_to(implementor, typed, [editor.prompt, editor.release_marker])
    released = reply_to(implementor, "/free", [editor.release_marker])
    assert editor.release_marker in released, released

    assert refusal(typed) in lines_of(reply), reply
    assert editor.prompt in reply, reply


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_shape_master_mobile_without_a_number_shows_its_usage(server) -> None:
    """Pins #318: `shape master_mobile` with no number names the current mob master, shows the
    usage line and leaves the master as it was."""
    implementor = server.login(IMPLEMENTOR.name)

    reply = implementor.command("shape master_mobile")

    reply_lines = lines_of(reply.text)
    assert "Usage: shape master_mobile <idnum>" in reply_lines, reply.text
    assert any(line.startswith("Mobile master is player #") for line in reply_lines), reply.text
    assert "Mobile permission set to player" not in reply.text, reply.text
