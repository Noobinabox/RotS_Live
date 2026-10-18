"""Poison over game hours: tainted bread hurts a mortal once per granted game hour for its
duration, then wears off."""

from __future__ import annotations

import re

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.poison]

# A level-20 human warrior in the arena: a mortal, since immortals are not poisoned by food.
MORTAL = Character(
    name="Testeater",
    race="human",
    sex="male",
    level=20,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="warrior",
)
# The tainted bread's virtual number in the test world (tests/integration/world/obj/11.obj).
TAINTED_BREAD_VNUM = 1139
# The bread's fill value is 2 and food poison lasts twice its fill, in game hours.
POISON_HOURS = 4
# Damage each poison tick does: affect_update_person (limits.cpp) calls
# damage(i, i, 5, SPELL_POISON, 0) once per game hour while the food poison's duration lasts.
# damage() (fight.cpp) leaves the 5 unchanged for this mortal: poison resists as RESIST_NONE
# and a human carries no resistance or vulnerability to it, nothing applies SPELL_SHIELD or
# SPELL_RESIST_POISON (which would shorten the duration), and the Beorning reduction is for
# Beornings only. point_update's own 5-point poison damage needs AFF_POISON without a
# SPELL_POISON affect, which never happens here: the bit is the affect's own and goes with it.
# Under affect-hours regeneration cannot raise full hit points before the first tick, so only
# that tick's damage reads exactly there.
POISON_TICK_DAMAGE = 5
# Damage all of the poison's ticks do together, with no regeneration to offset it.
TOTAL_POISON_DAMAGE = POISON_TICK_DAMAGE * POISON_HOURS
# A hunger low enough that eating is allowed (above 20 is too full) and high enough not to
# reach 0 in this test.
HUNGER_BEFORE_EATING = 10
# What the mortal reads after eating the bread.
EATEN_POISON_MARKER = "Oops, that tasted rather strange!"
# What the mortal reads on each poison tick.
POISON_TICK_MARKER = "You writhe in pain as poison courses through your veins."
# What the mortal reads when the poison wears off.
POISON_WEARS_OFF_MARKER = "You feel less sick."
# Seconds to keep reading after an acknowledgement, for text already sent to arrive.
SETTLE_SECONDS = 1.0
# The score line with current and maximum hit points.
HIT_POINTS_PATTERN = re.compile(r"You have (-?\d+)/(\d+) hit,")


def hit_points(session: GameSession) -> tuple:
    """The character's current and maximum hit points, read from score."""
    score = session.command("score").text
    match = HIT_POINTS_PATTERN.search(score)
    assert match is not None, f"no hit-point line in score:\n{score}"
    return int(match.group(1)), int(match.group(2))


def text_after_an_hour(server, session: GameSession, expected_marker: str = "") -> str:
    """Advances one game hour and returns what the session read during it."""
    server.advance_hours(1)
    if expected_marker:
        return session.expect([expected_marker]) + session.drain(SETTLE_SECONDS)
    return session.drain(SETTLE_SECONDS)


def poison_the_mortal_with_tainted_bread(server) -> tuple:
    """Logs in the implementor and MORTAL, has the mortal eat the tainted bread at full hit
    points, and returns the mortal's session and maximum hit points."""
    implementor = server.login(IMPLEMENTOR.name)
    mortal = server.login(MORTAL.name)
    mortal_keyword = MORTAL.name.lower()

    current_hit, maximum_hit = hit_points(mortal)
    assert current_hit == maximum_hit, (
        f"expected full hit points before eating, got {current_hit}/{maximum_hit}"
    )

    set_hunger = implementor.command(f"wizset {mortal_keyword} hunger {HUNGER_BEFORE_EATING}")
    assert set_hunger.contains(f"hunger set to {HUNGER_BEFORE_EATING}."), set_hunger.text
    implementor.command(f"goto {mortal_keyword}")
    assert implementor.command(f"load obj {TAINTED_BREAD_VNUM}").contains("You create")
    assert implementor.command(f"give bread {mortal_keyword}").contains("You give")
    assert mortal.command("eat bread").contains(EATEN_POISON_MARKER)
    return mortal, maximum_hit


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MORTAL)
def test_tainted_bread_poisons_a_mortal_once_per_game_hour_then_wears_off(server) -> None:
    mortal, maximum_hit = poison_the_mortal_with_tainted_bread(server)

    first_hour = text_after_an_hour(server, mortal, POISON_TICK_MARKER)
    assert first_hour.count(POISON_TICK_MARKER) == 1, first_hour
    assert hit_points(mortal) == (maximum_hit - POISON_TICK_DAMAGE, maximum_hit)

    for hour in range(2, POISON_HOURS + 1):
        during_hour = text_after_an_hour(server, mortal, POISON_TICK_MARKER)
        assert during_hour.count(POISON_TICK_MARKER) == 1, f"hour {hour}:\n{during_hour}"

    wearing_off = text_after_an_hour(server, mortal, POISON_WEARS_OFF_MARKER)
    assert POISON_TICK_MARKER not in wearing_off, wearing_off

    after_poison = text_after_an_hour(server, mortal)
    assert POISON_TICK_MARKER not in after_poison, after_poison
    assert POISON_WEARS_OFF_MARKER not in after_poison, after_poison


@pytest.mark.schedule("affect-hours-without-regen")
@pytest.mark.characters(IMPLEMENTOR, MORTAL)
def test_tainted_bread_damage_adds_up_tick_by_tick_without_regeneration(server) -> None:
    """The same poisoning as above with no regeneration, so every tick's damage stays: hit
    points fall by POISON_TICK_DAMAGE each hour and end TOTAL_POISON_DAMAGE below maximum."""
    mortal, maximum_hit = poison_the_mortal_with_tainted_bread(server)

    for hour in range(1, POISON_HOURS + 1):
        during_hour = text_after_an_hour(server, mortal, POISON_TICK_MARKER)
        assert during_hour.count(POISON_TICK_MARKER) == 1, f"hour {hour}:\n{during_hour}"
        expected_hit = maximum_hit - POISON_TICK_DAMAGE * hour
        assert hit_points(mortal) == (expected_hit, maximum_hit), f"after hour {hour}"

    wearing_off = text_after_an_hour(server, mortal, POISON_WEARS_OFF_MARKER)
    assert POISON_TICK_MARKER not in wearing_off, wearing_off

    after_poison = text_after_an_hour(server, mortal)
    assert POISON_TICK_MARKER not in after_poison, after_poison
    assert POISON_WEARS_OFF_MARKER not in after_poison, after_poison
    assert hit_points(mortal) == (maximum_hit - TOTAL_POISON_DAMAGE, maximum_hit)
