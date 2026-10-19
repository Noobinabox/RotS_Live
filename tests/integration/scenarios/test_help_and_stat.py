"""Small pins on help, stat and wizset: every spelling of `help color` shows the whole colour
entry, stat names the advanced-prompt preference, and wizset matches field names in any case."""

from __future__ import annotations

import re
from pathlib import Path

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.help_and_stat]

# A level-12 human warrior in the arena centre: the player who reads help, sets its prompt and
# has wizset change its gold.
PLAYER = Character(
    name="Testreader",
    race="human",
    sex="female",
    level=12,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="warrior",
)

# Every keyword the colour entry lists, each a spelling a player may type.
COLOUR_HELP_SPELLINGS = ("color", "colour", "colors", "colours")
# The colour entry's keyword line in lib/text/help_tbl, which do_help (act_info.cpp) prints first.
COLOUR_ENTRY_KEYWORD_LINE = "COLOR COLOUR COLORS COLOURS"
# The end of the slot list, naming the mob slot (#292).
MOB_SLOT_LIST_MARKER = "enemy description group magic weather mob"
# The sentence explaining the mob slot (#292).
MOB_SLOT_SENTENCE_MARKER = "'mob' colours the"
# The hex-format sentence #298 keeps on one line: a help line starting with '#' ends the entry.
HEX_FORMAT_MARKER = "Hex values must look like #RRGGBB."
# The two lines of the entry's closing sentence about true colour.
TRUE_COLOUR_TAIL_MARKERS = (
    "If your terminal does not fully support true colour, these colours may",
    "not display exactly as chosen.",
)
# What the pager prints after each full page (game loop prompts, comm.cpp).
PAGER_PROMPT = "*** Press return to continue, q to quit ***"
# Lines on each page the pager shows (show_string, modify.cpp).
PAGE_LINES = 22
# The start of what do_help answers when no keyword matches.
NO_ENTRY_MARKER = "There is no entry for"
# What the player reads after turning the advanced prompt on (tog_messages, act_othe.cpp).
ADVANCED_PROMPT_ON_MARKER = "Advanced prompt is now on."
# The name stat gives PRF_ADVANCED_PROMPT, bit 31 of the preference flags (preference_bits,
# consts.cpp).
ADVANCED_PROMPT_FLAG_NAME = "ADVPRMPT"
# What sprintbit (utility.cpp) printed for a flag word with bit 31 set before #332.
SPRINTBIT_ERROR_MARKER = "SPRINTBIT ERROR"
# The stat line just before the affect lines (do_stat_character, act_wiz.cpp): once it arrives,
# the drain collects the rest of the reply.
STAT_LAST_LINE_MARKER = "VUL:"
# The stat line with the preference flags (do_stat_character): names separated by spaces, then
# the period sprintbit adds.
PREFERENCE_LINE_PATTERN = re.compile(r"PRF: ([^\r\n]*)")
# What wizset answers a field name that matches no row (do_wizset, act_wiz.cpp).
UNKNOWN_FIELD_MARKER = "Can't set that!"
# Seconds to keep reading after a reply, for text already sent to arrive.
SETTLE_SECONDS = 1.0


def colour_entry_lines(lib_dir: Path) -> list:
    """The colour entry as do_help sends it: from its keyword line up to the next line starting with '#'."""
    help_lines = (lib_dir / "text" / "help_tbl").read_text().splitlines()
    first = help_lines.index(COLOUR_ENTRY_KEYWORD_LINE)
    after = next(number for number in range(first + 1, len(help_lines)) if help_lines[number].startswith("#"))
    return help_lines[first:after]


def read_help_entry(session: GameSession, topic: str, entry_lines: list) -> str:
    """Asks for help on topic and pages through the reply; returns all of it.

    The reply ends at the entry's last non-blank line (or at the no-entry answer); every page
    before it ends at the pager prompt and is followed by a Return. The entry's own length
    bounds the pages.
    """
    last_line = next(line for line in reversed(entry_lines) if line.strip())
    page_limit = len(entry_lines) // PAGE_LINES + 1
    session.send_line(f"help {topic}")
    text = ""
    for page_number in range(1, page_limit + 2):
        page = session.expect([PAGER_PROMPT, last_line, NO_ENTRY_MARKER])
        text += page
        if PAGER_PROMPT not in page:
            return text + session.drain(SETTLE_SECONDS)
        session.send_line("")
    raise AssertionError(f"help {topic} ran past {page_number} pages:\n{text}")


@pytest.mark.characters(IMPLEMENTOR, PLAYER)
def test_every_spelling_of_help_color_shows_the_whole_colour_entry(server) -> None:
    """Pins #292 and #298: color, colour, colors and colours each page through the whole entry.

    do_help (act_info.cpp) finds the topic by prefix in the sorted keyword index and sends the
    entry's lines up to the next line starting with '#'. #292 made the four spellings this
    entry's keywords and added the mob slot; #298 kept "#RRGGBB" off the start of a line, where
    it ended the entry before its closing sentence about true colour.
    """
    player = server.login(PLAYER.name)
    entry_lines = colour_entry_lines(server.lib_dir)

    for spelling in COLOUR_HELP_SPELLINGS:
        entry = read_help_entry(player, spelling, entry_lines)
        assert NO_ENTRY_MARKER not in entry, entry
        assert COLOUR_ENTRY_KEYWORD_LINE in entry, entry
        assert MOB_SLOT_LIST_MARKER in entry, entry
        assert MOB_SLOT_SENTENCE_MARKER in entry, entry
        assert HEX_FORMAT_MARKER in entry, entry
        for tail_marker in TRUE_COLOUR_TAIL_MARKERS:
            assert tail_marker in entry, entry


@pytest.mark.characters(IMPLEMENTOR, PLAYER)
def test_stat_names_the_advanced_prompt_preference(server) -> None:
    """Pins #332: stat's PRF line names ADVPRMPT, the flag in bit 31, and never prints SPRINTBIT ERROR.

    sprintbit (utility.cpp) read a flag word with bit 31 set as negative and printed
    "SPRINTBIT ERROR!" in place of every flag; #332 reads it as unsigned 32 bits, and
    preference_bits (consts.cpp) names the bit ADVPRMPT.
    """
    implementor = server.login(IMPLEMENTOR.name)
    player = server.login(PLAYER.name)

    turned_on = player.command("set advancedprompt on")
    assert turned_on.contains(ADVANCED_PROMPT_ON_MARKER), turned_on.text

    implementor.send_line(f"stat player {PLAYER.name.lower()}")
    stat = implementor.expect([STAT_LAST_LINE_MARKER]) + implementor.drain(SETTLE_SECONDS)
    assert SPRINTBIT_ERROR_MARKER not in stat, stat
    preference_line = PREFERENCE_LINE_PATTERN.search(stat)
    assert preference_line is not None, stat
    preference_names = preference_line.group(1).rstrip(".").split()
    assert ADVANCED_PROMPT_FLAG_NAME in preference_names, preference_line.group(0)


@pytest.mark.characters(IMPLEMENTOR, PLAYER)
def test_wizset_matches_field_names_in_any_case(server) -> None:
    """Pins #276: wizset finds its field whatever the case the immortal types it in.

    do_wizset (act_wiz.cpp) matches the typed field against its table by prefix; #276 made the
    match case-insensitive, so the upper-case GOLD finds the "gold" row and the lower-case ob
    finds the "OB" row. The reply names the row as the table spells it. No draw is on the path.
    """
    implementor = server.login(IMPLEMENTOR.name)
    server.login(PLAYER.name)
    player_name = PLAYER.name.lower()

    gold = implementor.command(f"wizset player {player_name} GOLD 100")
    assert not gold.contains(UNKNOWN_FIELD_MARKER), gold.text
    assert gold.contains(f"{PLAYER.name}'s gold set to 100."), gold.text

    offence = implementor.command(f"wizset player {player_name} ob 5")
    assert not offence.contains(UNKNOWN_FIELD_MARKER), offence.text
    assert offence.contains(f"{PLAYER.name}'s OB set to 5."), offence.text
