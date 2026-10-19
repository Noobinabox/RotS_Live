"""Tame's zone slot: a tamed animal frees its zone reset slot once, not again when it dies."""

from __future__ import annotations

import re

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.tame]

# A level-30 human ranger in the boar pen who knows tame fully: 100 practice sessions put its
# knowledge at the cap (recalc_skills, spec_pro.cpp).
TAMER = Character(
    name="Testranger",
    race="human",
    sex="male",
    level=30,
    load_room=world.ROOM_BOAR_PEN,
    class_name="ranger",
    skills=(("tame", 100),),
)

# The pen boar's keyword (mob 1450, which zone 18 resets into the pen with max existing 1).
BOAR_KEYWORD = "boar"
# The pen boar's short description, as `where` prints it.
BOAR_NAME = "a pen boar"
# The test world's zone 18, the boar pen.
BOAR_ZONE = 18
# The end of the implementor's reply to `zreset 18` (do_zreset, act_wiz.cpp); the number before
# it is the zone's place in the zone table, not its vnum.
ZONE_RESET_MARKER = ": Harness boar pen."

# What everyone in the room but the boar reads when the tame succeeds (do_tame, ranger.cpp).
TAMED_MARKER = "A pen boar seems tamed."
# What the tamer reads when the tame roll fails (do_tame). The boar's answering attack (do_hit,
# act_offe.cpp) is refused in the pen, a PEACEROOM, so the boar stays calm for the next attempt.
FAILED_TAME_MARKER = "You fail to tame your target."
# Every other reply do_tame can give, in the order it checks them.
TAME_REFUSAL_MARKERS = [
    "You are too exhausted.",
    "Tame who?",
    "Your victim is no longer among us.",
    "You are too airy for that.",
    "You can only tame animals.",
    "Your target is too enraged!",
    "Your target needs to be standing.",
    "Your target is already following someone.",
    "Your target is too powerful for you to tame.",
    "Learn how to tame properly first!",
    "You are too busy to do that.",
    "You target is not here any longer.",
    "Sorry, following in circles is not allowed.",
]
# The most tame attempts made before the test fails. A failed attempt uses nothing up, so the
# bound is a count: at 8 failures in 101 per attempt (see the margin comment in the test), 20
# attempts all fail about once in 10^22 runs.
MAX_TAME_ATTEMPTS = 20
# Seconds to wait for a tame's outcome: 60 beats * boar level 1 * 2 / (ranger level 30 + 1) is
# 3 passes; 8 leaves room for a loaded host or emulated i386.
TAME_TIMEOUT = 8.0
# Seconds to keep reading after an outcome, for text already sent to arrive.
SETTLE_SECONDS = 1.0

# The stat line just before the affect lines (do_stat_character, act_wiz.cpp).
STAT_LAST_LINE_MARKER = "VUL:"
# The tamer's ranger level, which tame's success chance reads.
RANGER_LEVEL_PATTERN = re.compile(r"Class levels: Mag:\d+ Cle:\d+ Ran:(\d+) ")
# One line of the immortal `where <keyword>` list for the boar (perform_immort_where,
# act_info.cpp): `%3d. %-25s - [%5d] %s`, the room's vnum in brackets.
BOAR_WHERE_LINE_PATTERN = re.compile(r"^\s*\d+\. " + re.escape(BOAR_NAME) + r"\s+- \[\s*(\d+)\]", re.MULTILINE)


def boar_rooms(implementor: GameSession) -> list:
    """The room vnum of every pen boar in the world, from the implementor's `where boar`."""
    reply = implementor.command(f"where {BOAR_KEYWORD}")
    return sorted(int(room) for room in BOAR_WHERE_LINE_PATTERN.findall(reply.text))


def reset_boar_zone(implementor: GameSession) -> None:
    """Resets zone 18 at once through the implementor's `zreset`."""
    reply = implementor.command(f"zreset {BOAR_ZONE}")
    assert reply.contains(ZONE_RESET_MARKER), reply.text


def tame_boar(tamer: GameSession) -> str:
    """Tames the boar in the tamer's room; returns what the tamer read on success.

    The success roll is the one draw on the path, and a failure only gates the attempt, so the
    attempt is repeated after FAILED_TAME_MARKER, at most MAX_TAME_ATTEMPTS times.
    """
    outcomes = [TAMED_MARKER, FAILED_TAME_MARKER, *TAME_REFUSAL_MARKERS]
    for _attempt in range(MAX_TAME_ATTEMPTS):
        tamer.send_line(f"tame {BOAR_KEYWORD}")
        reply = tamer.expect(outcomes, timeout=TAME_TIMEOUT) + tamer.drain(SETTLE_SECONDS)
        if FAILED_TAME_MARKER not in reply:
            return reply
    pytest.fail(f"the tame failed {MAX_TAME_ATTEMPTS} times running")


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, TAMER)
def test_a_tamed_animal_frees_its_zone_slot_once_so_resets_keep_one_wild_copy(server) -> None:
    """New-behaviour pin: purging a tamed animal does not free its zone slot a second time.

    #314 made do_tame (ranger.cpp) clear the tame's load line after releasing its slot, so
    extract_char (handler.cpp) no longer releases the same slot again when the tame dies. Before
    it, the purge dropped the reset row's existing count below the wild copy it still had, and
    the next reset loaded a second wild boar. Zone 18 resets one boar with max existing 1;
    zreset runs inside the command, so the affect-hours schedule's lack of zone updates does not
    matter and keeps every other draw away. The tame's success roll is the one draw on the path,
    a gating roll that tame_boar repeats.
    """
    implementor = server.login(IMPLEMENTOR.name)
    tamer = server.login(TAMER.name)

    assert boar_rooms(implementor) == [world.ROOM_BOAR_PEN]

    implementor.send_line(f"stat player {TAMER.name.lower()}")
    stat_text = implementor.expect([STAT_LAST_LINE_MARKER]) + implementor.drain(SETTLE_SECONDS)
    ranger_level = RANGER_LEVEL_PATTERN.search(stat_text)
    assert ranger_level is not None, stat_text
    # Tame's success roll, Testranger (tame knowledge 100, the cap 100 practices reach):
    #   tmp_abil = (int)(GET_SKILL / 110.0 * 95 + Ran / 36.0 * 9)          (do_tame, ranger.cpp)
    #            = (int)(100 / 110.0 * 95    + 30 / 36.0 * 9)              (Ran:30)
    #              (no anger affect, so no 20% cut)
    #            = (int)(86.36 + 7.5) = 93
    #   tamed    = tmp_abil > number(0, 100): 93 of 101 draws succeed, NOT roll-proof, so a
    #              failure is retried. The level check passes: (int)(30 / 3.0 + 100 / 30.0)
    #              - boar level 1 - no followers = 13 - 1 - 0 = 12, not below 0.
    assert int(ranger_level.group(1)) == 30, stat_text
    assert int(100 / 110 * 95 + int(ranger_level.group(1)) / 36 * 9) == 93

    tamed = tame_boar(tamer)
    assert TAMED_MARKER in tamed, tamed

    # The tame follows its tamer out of the pen, so where tells it from the wild copy by room.
    tamer.command("east")
    assert boar_rooms(implementor) == [world.ROOM_PEN_GATE]

    reset_boar_zone(implementor)
    assert boar_rooms(implementor) == [world.ROOM_BOAR_PEN, world.ROOM_PEN_GATE]

    implementor.command(f"goto {world.ROOM_PEN_GATE}")
    purged = implementor.command(f"purge {BOAR_KEYWORD}")
    assert purged.contains("Ok."), purged.text
    assert boar_rooms(implementor) == [world.ROOM_BOAR_PEN]

    reset_boar_zone(implementor)
    reset_boar_zone(implementor)
    assert boar_rooms(implementor) == [world.ROOM_BOAR_PEN]
