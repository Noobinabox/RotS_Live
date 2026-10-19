"""Protection and resistances: a mystic's protection from dark and `unprotect`, and the strongest
worn item resistance applying whatever order the items go on in."""

from __future__ import annotations

import re

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.resistances]

# A level-20 human mystic in the arena centre who knows protection: ten practice sessions put
# protection's knowledge at the cap of 100 (learn_diff 1, recalc_skills in spec_pro.cpp).
MYSTIC = Character(
    name="Testmystic",
    race="human",
    sex="female",
    level=20,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="mystic",
    skills=(("protection", 10),),
)
# A level-20 human warrior in the arena centre who wears the fire-resist fixtures.
WEARER = Character(
    name="Testwearer",
    race="human",
    sex="male",
    level=20,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="warrior",
)

# The spirit the implementor gives the mystic: protection costs 5 (skills[] in consts.cpp) and a
# lost concentration 3, so this pays for every cast a test makes and leaves room for losses.
MYSTIC_SPIRIT = 40
# The mystic's mystic (Cle) profession level: a single-class mystic's is its overall level, up
# to 30.
MYSTIC_CLERIC_LEVEL = MYSTIC.level

# The fire-resist fixtures (tests/integration/world/obj/14.obj): a head item granting 20% and a
# neck item granting 40%, each through `A 27 <strength * 256 + 161>` (APPLY_SPELL with
# SPELL_RESIST_FIRE, spells.h). The worn markers are what wearing each prints (wear_message,
# act_obj2.cpp).
WEAK_ITEM_VNUM = 1400
WEAK_ITEM_KEYWORD = "circlet"
WEAK_ITEM_SHORT = "a fire circlet"
WEAK_ITEM_WORN_MARKER = "You wear a fire circlet on your head."
WEAK_ITEM_PERCENT = 20
STRONG_ITEM_VNUM = 1401
STRONG_ITEM_KEYWORD = "pendant"
STRONG_ITEM_SHORT = "a fire pendant"
STRONG_ITEM_WORN_MARKER = "You wear a fire pendant around your neck."
STRONG_ITEM_PERCENT = 40

# What the caster reads when the concentration roll fails (do_cast): number(0, 100) >= 100 still
# happens once in 101 casts at full knowledge.
LOST_CONCENTRATION_MARKER = "You lost your concentration!"
# What the caster reads when its spirit no longer covers a cast (can_cast_spell, spell_pa.cpp).
NO_ENERGY_MARKER = "You can't summon enough energy to cast the spell."
# What the protected character reads (spell_protection, mystic.cpp, case 5).
DARK_PROTECTION_MARKER = "You feel resistant to dark!"
# What a caster already protected reads on a second cast (spell_protection).
ALREADY_PROTECTED_MARKER = "You have protection already."
# What a caster reads for a sphere protection does not know (spell_protection); the line breaks
# after "illusion".
WRONG_SPHERE_MARKER = "You can master protection from fire, cold, lightning, physical, illusion\n\ror dark only."
# The start of that reply, which cast_protection waits for so that a changed sphere list fails on
# the assertion rather than as a timeout.
WRONG_SPHERE_START = "You can master protection from"
# What the character and its room read when it drops its protection (do_unprotect, mystic.cpp);
# format the room's line with the name.
RENOUNCED_MARKER = "You renounce your protection!"
RENOUNCED_ROOM_MARKER_FORMAT = "{} renounces her protection!"
# What unprotect prints when there is no protection to drop (do_unprotect).
NOTHING_TO_RENOUNCE_MARKER = "You renounce yourself to the world, but nothing happens..."
# What affections prints for a character with no affect or resistance (do_affections,
# act_info.cpp).
NOT_AFFECTED_MARKER = "You are not affected by anything."
# What the wearer reads when an item installs its fire resistance (do_resist_spell, mystic.cpp).
FIRE_RESISTANT_MARKER = "You feel resistant to fire!"
# The line affections prints above the resistance lines (do_affections, act_info.cpp).
RESISTANT_HEADER = "You are resistant to:"
# One resistance line in affections (sprintbit_resistances, utility.cpp); format with the
# element and the strength in percent.
RESISTANCE_LINE_FORMAT = "   {} ({}%)"
# What the wearer reads on taking an item off (perform_remove, act_obj2.cpp); format with the
# item's short description.
STOP_USING_MARKER_FORMAT = "You stop using {}."
# Seconds to wait for a cast's outcome: protection's 21 beats at mystic level 20 take a few
# seconds (CASTING_TIME, spells.h); 15 leaves room for a loaded host or emulated i386.
CAST_TIMEOUT = 15.0
# Seconds to keep reading after an outcome, for text already sent to arrive.
SETTLE_SECONDS = 1.0
# The stat line just before the affect lines (do_stat_character, act_wiz.cpp): once it arrives,
# the drain collects the rest of the reply.
STAT_LAST_LINE_MARKER = "VUL:"

# The stat line with the profession levels (do_stat_character, act_wiz.cpp).
CLERIC_LEVEL_PATTERN = re.compile(r"Class levels: Mag:\d+ Cle:(\d+) ")


def give_spirit(implementor: GameSession) -> None:
    """Sets the mystic's spirit, the pool cleric spells are paid from."""
    reply = implementor.command(f"wizset player {MYSTIC.name.lower()} spirit {MYSTIC_SPIRIT}")
    assert reply.contains(f"spirit set to {MYSTIC_SPIRIT}."), reply.text


def mystic_cleric_level(implementor: GameSession) -> int:
    """The mystic profession level stat shows for the mystic."""
    implementor.send_line(f"stat player {MYSTIC.name.lower()}")
    text = implementor.expect([STAT_LAST_LINE_MARKER]) + implementor.drain(SETTLE_SECONDS)
    match = CLERIC_LEVEL_PATTERN.search(text)
    assert match is not None, f"no class levels line for {MYSTIC.name}:\n{text}"
    return int(match.group(1))


def cast_protection(caster: GameSession, sphere: str) -> str:
    """Casts protection from sphere on the caster; returns what the caster read after.

    A lost concentration is the one roll that can stop the cast, so the cast is repeated after
    it. Each loss costs spirit, and nothing regenerates spirit between requests under
    affect-hours, which every test here uses, so the repeats end at NO_ENERGY_MARKER at the latest.
    """
    outcomes = [
        DARK_PROTECTION_MARKER,
        ALREADY_PROTECTED_MARKER,
        WRONG_SPHERE_START,
        LOST_CONCENTRATION_MARKER,
        NO_ENERGY_MARKER,
    ]
    while True:
        caster.send_line(f"cast 'protection' {sphere}")
        reply = caster.expect(outcomes, timeout=CAST_TIMEOUT) + caster.drain(SETTLE_SECONDS)
        if LOST_CONCENTRATION_MARKER not in reply:
            return reply


def affections(session: GameSession) -> str:
    """What the character's affections command prints."""
    return session.command("affections").text


def protect_mystic_from_dark(server) -> tuple:
    """Logs in the implementor and the mystic, funds the mystic and has it cast protection dark.

    Returns the two sessions and what the mystic read after the cast.
    """
    implementor = server.login(IMPLEMENTOR.name)
    mystic = server.login(MYSTIC.name)
    give_spirit(implementor)
    return implementor, mystic, cast_protection(mystic, "dark")


def hand_out_fire_items(server) -> tuple:
    """Logs in the implementor and the wearer and gives the wearer both fire-resist items."""
    implementor = server.login(IMPLEMENTOR.name)
    wearer = server.login(WEARER.name)
    wearer_keyword = WEARER.name.lower()
    implementor.command(f"goto {wearer_keyword}")
    for vnum, keyword in ((WEAK_ITEM_VNUM, WEAK_ITEM_KEYWORD), (STRONG_ITEM_VNUM, STRONG_ITEM_KEYWORD)):
        loaded = implementor.command(f"load obj {vnum}")
        assert loaded.contains("You create"), loaded.text
        given = implementor.command(f"give {keyword} {wearer_keyword}")
        assert given.contains("You give"), given.text
    return implementor, wearer


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MYSTIC)
def test_a_mystic_casts_protection_from_dark_and_affections_shows_its_strength(server) -> None:
    """Protection from dark grants a dark resistance worth the mystic level plus ten (PR #342).

    The one draw that can stop the cast is the concentration roll in do_cast, which
    cast_protection repeats; the strength itself comes from the profession level, not from a
    roll (cast_resist_magnitude, mystic.cpp).
    """
    implementor, mystic, reply = protect_mystic_from_dark(server)
    assert DARK_PROTECTION_MARKER in reply, reply

    # Testmystic (level-20 mystic):
    #   effect_modifier = cast_resist_magnitude(get_prof_level(PROF_CLERIC, caster))  (mystic.cpp)
    #                   = (level > 30 ? 30 : level) + 10
    #                   = (20 > 30 ? 30 : 20) + 10        (Class levels: Cle:20)
    #                   = 30
    cleric_level = mystic_cleric_level(implementor)
    assert cleric_level == MYSTIC_CLERIC_LEVEL, cleric_level
    expected_percent = min(cleric_level, 30) + 10
    assert expected_percent == 30

    shown = affections(mystic)
    assert RESISTANT_HEADER in shown, shown
    assert RESISTANCE_LINE_FORMAT.format("dark", expected_percent) in shown, shown


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MYSTIC)
def test_a_mystic_already_protected_is_refused_a_second_protection(server) -> None:
    """A second protection cast while one holds is refused with its own message (PR #342).

    The one draw that can stop either cast is the concentration roll in do_cast, which
    cast_protection repeats.
    """
    _, mystic, first_reply = protect_mystic_from_dark(server)
    assert DARK_PROTECTION_MARKER in first_reply, first_reply

    second_reply = cast_protection(mystic, "dark")
    assert ALREADY_PROTECTED_MARKER in second_reply, second_reply
    assert DARK_PROTECTION_MARKER not in second_reply, second_reply


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MYSTIC)
def test_protection_names_dark_among_its_spheres_when_given_one_it_does_not_know(server) -> None:
    """Protection from an unknown sphere lists the six it knows, dark last (PR #342).

    The one draw that can stop the cast is the concentration roll in do_cast, which
    cast_protection repeats.
    """
    implementor = server.login(IMPLEMENTOR.name)
    mystic = server.login(MYSTIC.name)
    give_spirit(implementor)

    reply = cast_protection(mystic, "water")
    assert WRONG_SPHERE_MARKER in reply, reply
    assert NOT_AFFECTED_MARKER in affections(mystic)


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MYSTIC)
def test_unprotect_drops_protection_and_a_second_unprotect_does_nothing(server) -> None:
    """unprotect drops a protection and tells the room; without one it does nothing (PR #342).

    The one draw that can stop the cast is the concentration roll in do_cast, which
    cast_protection repeats; unprotect itself draws nothing.
    """
    implementor, mystic, reply = protect_mystic_from_dark(server)
    assert DARK_PROTECTION_MARKER in reply, reply
    implementor.command(f"goto {MYSTIC.name.lower()}")

    renounced = mystic.command("unprotect")
    assert renounced.contains(RENOUNCED_MARKER), renounced.text
    room_marker = RENOUNCED_ROOM_MARKER_FORMAT.format(MYSTIC.name)
    seen = implementor.expect([room_marker]) + implementor.drain(SETTLE_SECONDS)
    assert room_marker in seen, seen
    assert NOT_AFFECTED_MARKER in affections(mystic)

    again = mystic.command("unprotect")
    assert again.contains(NOTHING_TO_RENOUNCE_MARKER), again.text
    assert not again.contains(RENOUNCED_MARKER), again.text


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, WEARER)
@pytest.mark.parametrize("strong_first", [False, True], ids=["weak-first", "strong-first"])
def test_the_strongest_worn_fire_resistance_applies_whatever_the_wear_order(server, strong_first) -> None:
    """Between two worn fire-resist items the stronger holds the resistance (PR #315).

    The weaker item put on beside the stronger one says nothing; the stronger one put on after
    the weaker one takes the slot and announces it. Wearing draws nothing.
    """
    _, wearer = hand_out_fire_items(server)
    weak = (WEAK_ITEM_KEYWORD, WEAK_ITEM_WORN_MARKER)
    strong = (STRONG_ITEM_KEYWORD, STRONG_ITEM_WORN_MARKER)
    first, second = (strong, weak) if strong_first else (weak, strong)

    first_worn = wearer.command(f"wear {first[0]}")
    assert first_worn.contains(first[1]), first_worn.text
    assert first_worn.contains(FIRE_RESISTANT_MARKER), first_worn.text

    second_worn = wearer.command(f"wear {second[0]}")
    assert second_worn.contains(second[1]), second_worn.text
    assert second_worn.contains(FIRE_RESISTANT_MARKER) == (not strong_first), second_worn.text

    shown = affections(wearer)
    assert RESISTANCE_LINE_FORMAT.format("fire", STRONG_ITEM_PERCENT) in shown, shown
    assert RESISTANCE_LINE_FORMAT.format("fire", WEAK_ITEM_PERCENT) not in shown, shown


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, WEARER)
def test_removing_the_stronger_fire_item_falls_back_quietly_to_the_weaker(server) -> None:
    """Taking off the item holding the resistance hands it to the strongest still worn (PR #315).

    The wearer never lost the resistance, so the removal prints no resistance message.
    Wearing and removing draw nothing.
    """
    _, wearer = hand_out_fire_items(server)
    assert wearer.command(f"wear {WEAK_ITEM_KEYWORD}").contains(FIRE_RESISTANT_MARKER)
    assert wearer.command(f"wear {STRONG_ITEM_KEYWORD}").contains(FIRE_RESISTANT_MARKER)

    removed = wearer.command(f"remove {STRONG_ITEM_KEYWORD}")
    assert removed.contains(STOP_USING_MARKER_FORMAT.format(STRONG_ITEM_SHORT)), removed.text
    assert not removed.contains(FIRE_RESISTANT_MARKER), removed.text

    shown = affections(wearer)
    assert RESISTANCE_LINE_FORMAT.format("fire", WEAK_ITEM_PERCENT) in shown, shown
    assert RESISTANCE_LINE_FORMAT.format("fire", STRONG_ITEM_PERCENT) not in shown, shown
