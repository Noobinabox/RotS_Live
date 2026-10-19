"""Mob scripts run as written: a false IF skips its whole block, nested blocks included, section
rows change nothing, and a script that has finished stays finished when its mob is killed."""

from __future__ import annotations

import re
from dataclasses import dataclass

import pytest

from rots_harness.characters import IMPLEMENTOR
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.scripts]


@dataclass(frozen=True)
class Smith:
    """A test-world mob whose ON_RECEIVE script checks for the star token."""

    # The mob's virtual number (tests/integration/world/mob/17.mob).
    vnum: int
    # The keyword that finds this mob and no other in the room.
    keyword: str
    # The mob's short description (tests/integration/world/mob/17.mob).
    short_description: str


# Script 1700 (tests/integration/world/scr/17.scr): Brali's shape (#2709), an outer IF on the
# star token whose block holds a nested guard IF / BEGIN / ... / END_ELSE_BEGIN / END with an
# empty else branch, then the reward; the outer else branch and a final line follow.
PLAIN_SMITH = Smith(vnum=1700, keyword="plain", short_description="a plain smith")
# Script 1701: script 1700 with SECTION rows between the outer IF and its BEGIN, before the
# nested IF, and on the nested block's empty else branch.
LABELLED_SMITH = Smith(vnum=1701, keyword="labelled", short_description="a labelled smith")
SMITHS = [pytest.param(PLAIN_SMITH, id="plain"), pytest.param(LABELLED_SMITH, id="labelled")]

# The star token and a grey pebble (tests/integration/world/obj/17.obj).
STAR_TOKEN_VNUM = 1700
PEBBLE_VNUM = 1701
# The weary foreman (mob 1702), whose script 1702 says a line when anyone speaks, waits four
# passes (DO_WAIT), says a last line and ends.
FOREMAN_VNUM = 1702
FOREMAN_SHORT_DESCRIPTION = "a weary foreman"

# What the smiths say (DO_SAY rows of scripts 1700 and 1701), in script order: the nested
# guard's body, which a player giver never reaches; the reward inside the star token's block;
# the outer else branch; and the last row, which every give reaches.
GUARD_LINE = "I do not trade with your kind."
REWARD_LINE = "A star token! Here is your reward."
NOT_A_TOKEN_LINE = "That is no star token."
FINAL_LINE = "Who is next?"
# What the foreman says before and after its wait (script 1702).
FAREWELL_LINE = "Goodbye, then."
LAST_LINE = "My shift is over at last."
# What the implementor reads when it slays the foreman (do_slay, act_offe.cpp), before
# raw_kill runs.
SLAY_MARKER = "You chop him to pieces!  Ah!  The blood!"
# What the implementor reads from raw_kill's death_cry (fight.cpp), after the point where a
# repeated last line would have been said.
DEATH_CRY_MARKER = f"Your blood freezes as you hear {FOREMAN_SHORT_DESCRIPTION}'s death cry."
# The stat line that holds a character's delay (do_stat_character, act_wiz.cpp).
DELAY_LINE_PATTERN = re.compile(r"Delay command:(-?\d+) delay_value:(-?\d+)")
# The stat line after the delay line's block; once it arrives the drain collects the rest.
STAT_LAST_LINE_MARKER = "VUL:"
# Seconds to wait for the foreman's last line: four passes are one second (OPT_USEC,
# comm.cpp); the rest leaves room for a loaded host.
WAIT_TIMEOUT = 10.0
# Seconds to keep reading after an outcome, for text already sent to arrive.
SETTLE_SECONDS = 1.0


def said(short_description: str, line: str) -> str:
    """What the implementor reads when a mob says a line (say_to_char, act_comm.cpp).

    act() capitalises the first letter of the line it sends.
    """
    return f"{short_description[0].upper()}{short_description[1:]} says '{line}'"


def load_mob(implementor: GameSession, vnum: int, short_description: str) -> None:
    """Loads a mob into the implementor's room and checks the game's reply."""
    reply = implementor.command(f"load mob {vnum}")
    assert reply.contains(f"You create {short_description}."), reply.text


def give_new_object(implementor: GameSession, vnum: int, keyword: str, smith: Smith) -> str:
    """Loads an object and gives it to the smith; returns the give's reply.

    The ON_RECEIVE trigger runs inside the give (perform_give, act_obj1.cpp), so everything the
    smith's script says arrives in the reply, before its prompt.
    """
    assert implementor.command(f"load obj {vnum}").contains("You create")
    reply = implementor.command(f"give {keyword} {smith.keyword}")
    assert reply.contains("You give"), reply.text
    return reply.text


def finish_the_foremans_script(implementor: GameSession) -> None:
    """Loads the foreman, starts its script by speaking, and waits for the script's last line.

    The first line runs inside the say (do_say, act_comm.cpp). The last runs when the foreman's
    wait ends on the game loop's waiting list (comm.cpp), which runs every pass under the
    affect-hours schedule too.
    """
    load_mob(implementor, FOREMAN_VNUM, FOREMAN_SHORT_DESCRIPTION)
    spoken = implementor.command("say Your shift is done.")
    assert said(FOREMAN_SHORT_DESCRIPTION, FAREWELL_LINE) in spoken.text, spoken.text
    last_line_said = said(FOREMAN_SHORT_DESCRIPTION, LAST_LINE)
    finished = implementor.expect([last_line_said], timeout=WAIT_TIMEOUT)
    finished += implementor.drain(SETTLE_SECONDS)
    assert finished.count(last_line_said) == 1, finished


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
@pytest.mark.parametrize("smith", SMITHS)
def test_a_false_if_skips_its_whole_block_with_the_nested_block_inside(server, smith: Smith) -> None:
    """A false outer IF skips its block, nested IF/ELSE included, and runs its else branch.

    Pins #305: get_next_command (script.cpp) skips a nested IF / BEGIN / ... /
    END_ELSE_BEGIN / END with an empty else branch as a whole, so giving a smith anything but
    the star token runs the outer else branch and never the reward (Brali's script #2709 has
    this shape). The labelled smith pins #368 on the same path: section rows between the IF
    and its BEGIN and on the nested empty else branch leave the skip where it is. Nothing here
    draws a random number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    load_mob(implementor, smith.vnum, smith.short_description)

    reply = give_new_object(implementor, PEBBLE_VNUM, "pebble", smith)
    assert said(smith.short_description, FINAL_LINE) in reply, reply
    assert said(smith.short_description, NOT_A_TOKEN_LINE) in reply, reply
    assert REWARD_LINE not in reply, reply
    assert GUARD_LINE not in reply, reply


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
@pytest.mark.parametrize("smith", SMITHS)
def test_a_true_if_runs_its_block_past_the_skipped_nested_guard(server, smith: Smith) -> None:
    """A true outer IF runs its block: the false nested guard is skipped and the reward runs.

    Pins the other side of #305: inside the star token's block the nested guard is false for a
    player giver, its block is skipped, and the script goes on to the reward, then past the
    outer else branch to the last row. The labelled smith pins #368 here too: a section row on
    the path that runs, including the one labelling the empty else branch the guard's skip
    lands on, does nothing. Nothing here draws a random number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    load_mob(implementor, smith.vnum, smith.short_description)

    reply = give_new_object(implementor, STAR_TOKEN_VNUM, "token", smith)
    reward = said(smith.short_description, REWARD_LINE)
    final = said(smith.short_description, FINAL_LINE)
    assert reward in reply and final in reply, reply
    assert reply.index(reward) < reply.index(final), reply
    assert NOT_A_TOKEN_LINE not in reply, reply
    assert GUARD_LINE not in reply, reply


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_a_finished_script_leaves_no_script_delay_on_its_mob(server) -> None:
    """Once a waiting script has run to its end, the mob's delay command is cleared.

    Pins #270's continue_char_script (script.cpp), which clears delay.cmd before the script
    goes on after a DO_WAIT. Left set, the script's command (CMD_SCRIPT, 9999, interpre.h)
    stays on the mob with no wait to count down. Nothing here draws a random number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    finish_the_foremans_script(implementor)

    implementor.send_line("stat mob foreman")
    stat = implementor.expect([STAT_LAST_LINE_MARKER]) + implementor.drain(SETTLE_SECONDS)
    delay = DELAY_LINE_PATTERN.search(stat)
    assert delay is not None, stat
    assert (int(delay.group(1)), int(delay.group(2))) == (0, 0), stat


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_slaying_a_mob_whose_script_finished_does_not_repeat_its_last_line(server) -> None:
    """Killing a mob whose script has finished does not run the script's last line again.

    Pins #270: raw_kill (fight.cpp) completes a pending delay only while one is still counting
    down, and the finished script left none. A repeated line would arrive between the slay
    message and the death cry. Either of #270's two changes prevents it alone, so the pin fails
    only with both undone. slay calls raw_kill directly (do_slay, act_offe.cpp), with no roll.
    """
    implementor = server.login(IMPLEMENTOR.name)
    finish_the_foremans_script(implementor)

    reply = implementor.command("slay foreman")
    assert reply.contains(SLAY_MARKER), reply.text
    assert reply.contains(DEATH_CRY_MARKER), reply.text
    assert LAST_LINE not in reply.text, reply.text
