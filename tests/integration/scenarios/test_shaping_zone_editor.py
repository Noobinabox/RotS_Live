"""The zone editor: /implement reports the rows that name nothing, and its prompts, list range
and section rows."""

from __future__ import annotations

from pathlib import Path

import pytest

from rots_harness.characters import IMPLEMENTOR
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.shaping]

# The test world's zone 11, whose file has two M rows (tests/integration/world/zon/11.zon).
ZONE_NUMBER = 11
# A vnum no mob or object of the test world has.
MISSING_VNUM = 99999
# Where a new row lands: /6 selects row 2, the zone's last, and /10 inserts after it.
NEW_ROW_NUMBER = 3
# The zone file the editor loads and /save rewrites, relative to the lib (SHAPE_ZON_DIR, protos.h).
ZONE_FILE_RELATIVE = Path("world") / "zon" / f"{ZONE_NUMBER}.zon"
# The section row the section scenarios add: its title and its comment.
SECTION_TITLE = "Harness guards"
SECTION_COMMENT = "who waits where"
# The width of an editor list line (SHAPE_LIST_WIDTH, protos.h), and how much further in than its
# banner a section's comment starts in the zone list (show_command, shapezon.cpp).
LIST_WIDTH = 78
SECTION_COMMENT_INDENT = 2

# The editor's prompt while no field is being answered (prompt_text in consts.cpp).
ZONE_PROMPT = f"Zone: {ZONE_NUMBER}>"
# What the builder reads once shape zone has read the zone's rows (load_zone, shapezon.cpp).
ZONE_LOADED_MARKER = "Zone loaded, 2 commands found."
# What load_zone prints when the zone file or the zone in it is missing.
ZONE_FILE_MISSING_MARKER = "could not open that file."
ZONE_MISSING_MARKER = "no zone here."
# What /free prints (extra_coms_zone).
ZONE_RELEASED_MARKER = "You released your job and stop shaping."
# What /6 prints when it moves to the next row, and /10 when it inserts one.
NEXT_ROW_MARKER = "Next command chosen:"
ROW_INSERTED_MARKER = "New command added next to current, and selected."
# The /3 prompt's last line, the /4 prompt's for a new row's eight numbers, and the comment
# prompt's first words (shape_center_zone, zone_param_prompt).
COMMAND_TYPE_PROMPT = "Enter command type:"
EIGHT_NUMBERS_PROMPT = "Enter 8 numbers:"
COMMENT_PROMPT = "Enter line COMMENT"
# The section prompts' first words (shape_section_prompt, shapemob.cpp).
SECTION_TITLE_PROMPT = "Enter section TITLE"
SECTION_COMMENT_PROMPT = "Enter section COMMENT"
# What /3 prints for a letter it does not know.
UNRECOGNIZED_COMMAND_MARKER = "Unrecognized command."
# The start of the summary /implement ends with, with or without disabled rows (implement_zone).
IMPLEMENTED_MARKER = "The zone was implemented"
# The summary when every row resolved, or when no row was disabled.
IMPLEMENTED_CLEANLY = "The zone was implemented."
# The summary after one row was disabled.
IMPLEMENTED_WITH_ONE_DISABLED = (
    "The zone was implemented, but 1 command(s) were DISABLED because of the error(s) above."
    "  Fix the vnum(s) and implement again."
)
# What /implement prints instead when it cannot run.
NOT_ALLOWED_TO_IMPLEMENT_MARKER = "You are not allowed to implement this zone."
NO_SUCH_ZONE_MARKER = "It seems there is no such zone in the world"
# What syslog prints as it changes the builder's log level (do_syslog).
SYSLOG_NORMAL_MARKER = "Your syslog is now normal."
# A K row that kits the last mob with only the missing object; -1 marks the unused slots.
KIT_ROW_NUMBERS = f"0 {MISSING_VNUM} -1 -1 -1 -1 -1 -1"
# An M row loading the missing mob into the arena centre at full chance and normal xp.
MOB_ROW_NUMBERS = f"0 {MISSING_VNUM} 1131 0 100 100 0 0"
# Seconds to keep reading after a marker, for the editor prompt that follows it.
SETTLE_SECONDS = 0.5


def zone_error(command_letter: str, kind: str, disabled: bool) -> str:
    """The ZONE ERROR line renum_zone_one sends for the new row (zone_renum_report, zone.cpp)."""
    suffix = " - command disabled" if disabled else ""
    return (
        f"ZONE ERROR: zone #{ZONE_NUMBER}, command {NEW_ROW_NUMBER} ({command_letter}): "
        f"{kind} vnum {MISSING_VNUM} not found{suffix}"
    )


def lines_of(text: str) -> list:
    """The text's lines without the carriage returns the game sends with each line end."""
    return [line.strip("\r") for line in text.split("\n")]


def editor_step(implementor: GameSession, line: str, outcomes: list) -> str:
    """Sends one editor line and returns what came back, up to and just after the first outcome.

    Editor prompts end in ':' or ']' mid-field, so command() cannot be used inside the editor.
    """
    implementor.send_line(line)
    return implementor.expect(outcomes) + implementor.drain(SETTLE_SECONDS)


def open_zone_editor(implementor: GameSession) -> None:
    """Starts shaping zone 11, with row 1 selected."""
    loaded = editor_step(
        implementor,
        f"shape zone {ZONE_NUMBER}",
        [ZONE_LOADED_MARKER, ZONE_FILE_MISSING_MARKER, ZONE_MISSING_MARKER],
    )
    assert ZONE_LOADED_MARKER in loaded, loaded


def close_zone_editor(implementor: GameSession) -> None:
    """Leaves the editor, so the session can quit when the test ends."""
    released = editor_step(implementor, "/free", [ZONE_RELEASED_MARKER])
    assert ZONE_RELEASED_MARKER in released, released


def insert_row_after_the_last(implementor: GameSession, command_letter: str) -> str:
    """Inserts row 3 after the zone's last row and gives it command_letter; returns the reply.

    The reply ends at the row's next prompt: its numbers, or a section's title.
    """
    moved = editor_step(implementor, "/6", [NEXT_ROW_MARKER, "No next command."])
    assert NEXT_ROW_MARKER in moved, moved
    inserted = editor_step(implementor, "/10", [ROW_INSERTED_MARKER])
    assert ROW_INSERTED_MARKER in inserted, inserted
    typed = editor_step(implementor, "/3", [COMMAND_TYPE_PROMPT])
    assert COMMAND_TYPE_PROMPT in typed, typed
    return editor_step(
        implementor,
        command_letter,
        [EIGHT_NUMBERS_PROMPT, SECTION_TITLE_PROMPT, UNRECOGNIZED_COMMAND_MARKER],
    )


def enter_row_numbers_and_comment(implementor: GameSession, numbers: str, comment: str) -> None:
    """Answers the new row's number and comment prompts, then checks the row is selected."""
    numbered = editor_step(implementor, numbers, [COMMENT_PROMPT, ZONE_PROMPT])
    assert COMMENT_PROMPT in numbered, numbered
    implementor.send_line(comment)
    shown = editor_step(implementor, "/1", ["Curr:", "No current command."])
    assert f"Curr:   {NEW_ROW_NUMBER} " in shown, shown


def implement(implementor: GameSession) -> str:
    """Runs /implement and returns its report."""
    return editor_step(
        implementor,
        "/implement",
        [IMPLEMENTED_MARKER, NOT_ALLOWED_TO_IMPLEMENT_MARKER, NO_SUCH_ZONE_MARKER],
    )


def add_section_row(implementor: GameSession) -> None:
    """Inserts row 3 as a section with SECTION_TITLE and SECTION_COMMENT."""
    title_prompt = insert_row_after_the_last(implementor, "=")
    assert SECTION_TITLE_PROMPT in title_prompt, title_prompt
    comment_prompt = editor_step(implementor, SECTION_TITLE, [SECTION_COMMENT_PROMPT, ZONE_PROMPT])
    assert SECTION_COMMENT_PROMPT in comment_prompt, comment_prompt
    implementor.send_line(SECTION_COMMENT)
    shown = editor_step(implementor, "/1", ["Curr:", "No current command."])
    assert f"Curr:   {NEW_ROW_NUMBER} " in shown and SECTION_TITLE in shown, shown


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_implementing_a_kit_row_naming_a_missing_object_tells_the_builder(server) -> None:
    """Pins #312: /implement tells the builder which row names an object that does not exist.

    A K row is never disabled for a missing object, so the summary stays the clean one. The
    builder's syslog is off (rotstool's default), so the line comes straight to the builder
    rather than through the log (zone_renum_send, zone.cpp).
    """
    implementor = server.login(IMPLEMENTOR.name)
    syslog = implementor.command("syslog")
    assert syslog.contains("Your syslog is currently off."), syslog.text

    open_zone_editor(implementor)
    numbers_prompt = insert_row_after_the_last(implementor, "K")
    assert EIGHT_NUMBERS_PROMPT in numbers_prompt, numbers_prompt
    enter_row_numbers_and_comment(implementor, KIT_ROW_NUMBERS, "kit with a missing object")
    report = implement(implementor)
    close_zone_editor(implementor)

    report_lines = lines_of(report)
    assert zone_error("K", "object", disabled=False) in report_lines, report
    assert IMPLEMENTED_CLEANLY in report_lines, report


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_implementing_a_mob_row_naming_a_missing_mob_disables_it_and_says_so(server) -> None:
    """Pins #312: an M row naming a missing mob is disabled, its line says so, and the summary
    counts it."""
    implementor = server.login(IMPLEMENTOR.name)

    open_zone_editor(implementor)
    numbers_prompt = insert_row_after_the_last(implementor, "M")
    assert EIGHT_NUMBERS_PROMPT in numbers_prompt, numbers_prompt
    enter_row_numbers_and_comment(implementor, MOB_ROW_NUMBERS, "a mob that does not exist")
    report = implement(implementor)
    close_zone_editor(implementor)

    report_lines = lines_of(report)
    assert zone_error("M", "mobile", disabled=True) in report_lines, report
    assert IMPLEMENTED_WITH_ONE_DISABLED in report_lines, report


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_a_builder_whose_syslog_shows_zone_errors_reads_each_one_once(server) -> None:
    """Pins #312: with syslog at normal the ZONE ERROR line reaches the builder through the log,
    in mudlog's brackets, and is not sent a second time (mudlog_reaches, utility.cpp)."""
    implementor = server.login(IMPLEMENTOR.name)
    syslog = implementor.command("syslog normal")
    assert syslog.contains(SYSLOG_NORMAL_MARKER), syslog.text

    open_zone_editor(implementor)
    numbers_prompt = insert_row_after_the_last(implementor, "K")
    assert EIGHT_NUMBERS_PROMPT in numbers_prompt, numbers_prompt
    enter_row_numbers_and_comment(implementor, KIT_ROW_NUMBERS, "kit with a missing object")
    report = implement(implementor)
    close_zone_editor(implementor)

    error_line = zone_error("K", "object", disabled=False)
    assert report.count(error_line) == 1, report
    assert f"[ {error_line} ]" in lines_of(report), report


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_an_m_rows_number_prompt_says_an_xp_percent_of_100_is_normal(server) -> None:
    """Pins #319: the /4 prompt of an M row explains its xp% field, and shows the row's values."""
    implementor = server.login(IMPLEMENTOR.name)

    open_zone_editor(implementor)
    prompt = editor_step(implementor, "/4", ["Enter 8 numbers [", ZONE_PROMPT])
    kept = editor_step(implementor, "", ["Values kept.", ZONE_PROMPT])
    assert "Values kept." in kept, kept
    implementor.send_line("")
    close_zone_editor(implementor)

    prompt_lines = lines_of(prompt)
    assert "  max_world, max_alive_line: 0 = no limit.  xp%: 100 = normal (0 also normal)" in prompt_lines, prompt
    assert "Enter 8 numbers [0 1130 1132 0 100 100 1 1]:" in prompt_lines, prompt


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_a_list_range_limits_the_zone_list_and_its_footer_says_so(server) -> None:
    """Pins #320: /52 sets where the /50 list starts, and the list's last line names the range."""
    implementor = server.login(IMPLEMENTOR.name)

    open_zone_editor(implementor)
    range_prompt = editor_step(implementor, "/52", ["Enter list range:", ZONE_PROMPT])
    assert "Enter list range: start [end] (0 = all, blank keeps) [all]:" in lines_of(range_prompt), range_prompt
    range_set = editor_step(implementor, "2", ["List range set:", "Range not changed.", ZONE_PROMPT])
    assert "List range set: from 2." in lines_of(range_set), range_set
    listing = editor_step(implementor, "/50", [ZONE_PROMPT])
    close_zone_editor(implementor)

    listing_lines = lines_of(listing)
    assert "List range: from 2 (/52 to change)." in listing_lines, listing
    assert not any(line.startswith("  1 M::") for line in listing_lines), listing
    assert any(line.startswith("  2 M:: Ld_flg(0) Mob:1131 toRom:1134") for line in listing_lines), listing


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_a_section_row_shows_as_a_titled_banner_in_the_zone_list(server) -> None:
    """Pins #368: the /50 list shows a section row as its title centred in a row of '=', its
    comment underneath, and a closing row of '=' (shape_section_show, shapemob.cpp).

    After the row's label, the banner and the closing row fill the rest of the list's width.
    """
    implementor = server.login(IMPLEMENTOR.name)

    open_zone_editor(implementor)
    add_section_row(implementor)
    listing = editor_step(implementor, "/50", [ZONE_PROMPT])
    close_zone_editor(implementor)

    label = f"{NEW_ROW_NUMBER:3d} "
    banner_width = LIST_WIDTH - len(label)
    banner_middle = f" {SECTION_TITLE} "
    fill = banner_width - len(banner_middle)
    expected_rows = [
        label + "=" * (fill // 2) + banner_middle + "=" * (fill - fill // 2),
        " " * (len(label) + SECTION_COMMENT_INDENT) + SECTION_COMMENT,
        " " * len(label) + "=" * banner_width,
    ]
    listing_lines = lines_of(listing)
    first = listing_lines.index(expected_rows[0]) if expected_rows[0] in listing_lines else -1
    assert first >= 0 and listing_lines[first:first + 3] == expected_rows, listing


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_saving_a_zone_writes_its_section_row_to_the_zone_file(server) -> None:
    """Pins #368: /save writes a section row as "= <title> | <comment>" in its place among the
    zone's rows.

    /save copies the old file into world/zon/oldzons first, which the test world provides.
    """
    implementor = server.login(IMPLEMENTOR.name)
    zone_file = server.lib_dir / ZONE_FILE_RELATIVE

    open_zone_editor(implementor)
    add_section_row(implementor)
    save_reply = editor_step(implementor, "/save", ["shapeless indefinity"])
    # The game runs one line at a time, so once /1 answers, /save has finished writing.
    editor_step(implementor, "/1", ["Curr:"])
    close_zone_editor(implementor)

    assert "could not" not in save_reply, save_reply
    saved_text = zone_file.read_text(encoding="latin-1")
    saved_rows = [line for line in saved_text.splitlines() if line[:1] in ("M", "=")]
    assert saved_rows == [
        "M 0 1130 1132 0 100 100 1 1 target orc waits in arena east",
        "M 0 1131 1134 0 100 100 1 1 snake waits in corridor one",
        f"= {SECTION_TITLE} | {SECTION_COMMENT}",
    ], saved_text
