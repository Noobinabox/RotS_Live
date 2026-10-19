"""Identify's attribute lines: race-restriction and stay-zone flags each get their own line, and
the list ends without a lone full stop."""

from __future__ import annotations

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.identify]

# A level-30 human mage in the implementors' room who knows identify fully. Identify is a
# level-18 mage spell that targets an object the caster carries (skills[] in consts.cpp).
MAGE = Character(
    name="Testsage",
    race="human",
    sex="female",
    level=30,
    load_room=world.ROOM_IMMORTAL_START,
    class_name="mage",
    skills=(("identify", 100),),
)

# The dwarven stone's virtual number (tests/integration/world/obj/16.obj): its extra flags are
# DWARF (bit 18) and STAY_ZONE (bit 28), and nothing else.
DWARVEN_STONE_VNUM = 1602
STONE_KEYWORD = "stone"
# The maximum mana the implementor gives the mage: a fresh level-30 mage logs in with less than
# identify's 70 (USE_MANA, spells.h), and a lost concentration costs half of it (do_cast).
MAGE_MAXIMUM_MANA = 500

# What the implementor reads when the load and the give go through (do_load, act_wiz.cpp; give).
CREATED_MARKER = "You create a dwarven stone."
GIVEN_MARKER_FORMAT = "You give a dwarven stone to {}."
# The first line identify prints (do_identify_object, act_info.cpp).
IDENTIFIED_MARKER = "You feel certain the object you have is a dwarven stone."
# What the caster reads when the concentration roll fails (do_cast, spell_pa.cpp):
# number(0, 100) >= 100 still happens once in 101 casts at full knowledge.
LOST_CONCENTRATION_MARKER = "You lost your concentration!"
# What the caster reads when its mana no longer covers a cast (can_cast_spell, spell_pa.cpp).
NO_ENERGY_MARKER = "You can't summon enough energy to cast the spell."
# What the caster reads when it does not carry the target (report_wrong_target, interpre.cpp).
NO_TARGET_MARKER = "You don't have that."
# What the caster reads when it does not know the spell (can_cast_spell).
UNKNOWN_SPELL_MARKER = "You don't know this spell."
# The header of identify's attribute list and the lines the stone's two flags print, in bit
# order (sprintbit, utility.cpp, with extra_messages[], act_info.cpp).
ATTRIBUTES_HEADER = "This item has the following attributes:"
EXPECTED_ATTRIBUTE_LINES = ["Dwarves only", "It cannot leave this area"]

# Seconds to wait for a cast's outcome: identify's 50 beats at mage level 30 are 25
# (CASTING_TIME, spells.h), about three seconds; 15 leaves room for a loaded host.
CAST_TIMEOUT = 15.0
# Seconds to keep reading after an outcome, for text already sent to arrive.
SETTLE_SECONDS = 1.0


def cast_identify(caster: GameSession) -> str:
    """Casts identify on the stone; returns what the caster read once the spell went off.

    A lost concentration is the one roll that can stop the cast, so the cast is repeated after
    it. Each loss costs mana and nothing restores mana between requests under affect-hours, so
    the repeats end at NO_ENERGY_MARKER at the latest.
    """
    outcomes = [IDENTIFIED_MARKER, LOST_CONCENTRATION_MARKER, NO_ENERGY_MARKER, NO_TARGET_MARKER, UNKNOWN_SPELL_MARKER]
    while True:
        caster.send_line(f"cast 'identify' {STONE_KEYWORD}")
        reply = caster.expect(outcomes, timeout=CAST_TIMEOUT) + caster.drain(SETTLE_SECONDS)
        if LOST_CONCENTRATION_MARKER not in reply:
            return reply


def attribute_lines(reply: str) -> list:
    """The lines after the attribute header, up to the first blank line, stripped."""
    lines = [line.strip() for line in reply.splitlines()]
    assert ATTRIBUTES_HEADER in lines, reply
    attributes = []
    for line in lines[lines.index(ATTRIBUTES_HEADER) + 1:]:
        if not line:
            break
        attributes.append(line)
    return attributes


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MAGE)
def test_identify_lists_race_restriction_and_stay_zone_lines_without_a_lone_full_stop(server) -> None:
    """Pins #332: identify gives each race-restriction and STAY_ZONE flag its own line, and no
    line is a lone full stop.

    Before #332, extra_messages[] stopped at bit 15, so these flags printed empty lines and the
    list ended in a line holding only `.`. The one draw that can stop the cast is the
    concentration roll in do_cast, which cast_identify repeats; the affect-hours schedule keeps
    everything else from drawing.
    """
    implementor = server.login(IMPLEMENTOR.name)
    mage = server.login(MAGE.name)
    mage_keyword = MAGE.name.lower()
    # wizset calls maximum mana "maxstamina"; restore fills current mana to the new maximum.
    set_mana = implementor.command(f"wizset player {mage_keyword} maxstamina {MAGE_MAXIMUM_MANA}")
    assert set_mana.contains(f"{MAGE.name}'s maxstamina set to {MAGE_MAXIMUM_MANA}."), set_mana.text
    restored = implementor.command(f"restore {mage_keyword}")
    assert restored.contains("Done."), restored.text
    created = implementor.command(f"load obj {DWARVEN_STONE_VNUM}")
    assert created.contains(CREATED_MARKER), created.text
    given = implementor.command(f"give {STONE_KEYWORD} {mage_keyword}")
    assert given.contains(GIVEN_MARKER_FORMAT.format(MAGE.name)), given.text

    reply = cast_identify(mage)
    assert IDENTIFIED_MARKER in reply, reply
    assert attribute_lines(reply) == EXPECTED_ATTRIBUTE_LINES, reply
    assert "." not in [line.strip() for line in reply.splitlines()], reply
