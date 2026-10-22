"""Summon's pins: a player in a dark room, and the save bonus its zone distance gives."""

from __future__ import annotations

import math
import re
from pathlib import Path

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.summon]

# Room 1201, the only room of zone 12, whose map square is (3, 4).
ROOM_DISTANT_CELL = 1201
# Room 9001, the only room of zone 90, whose map square is (20, 0).
ROOM_PLUS_TWENTY_CELL = 9001
# Room 9101, the only room of zone 91, whose map square is (-20, 0).
ROOM_MINUS_TWENTY_CELL = 9101

# A level-30 human mage in the arena centre who knows summon fully: 100 practice sessions put
# its knowledge at the cap of 100 (recalc_skills, spec_pro.cpp).
CASTER = Character(
    name="Testsummoner",
    race="human",
    sex="female",
    level=30,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="mage",
    skills=(("summon", 100),),
)
# A level-12 human warrior who starts in the dark cell: a mortal on the caster's side.
DARK_VICTIM = Character(
    name="Testlost",
    race="human",
    sex="male",
    level=12,
    load_room=world.ROOM_DARK_CELL,
    class_name="warrior",
)
# A level-12 human warrior who starts in zone 12, five map squares from the caster's zone.
DISTANT_VICTIM = Character(
    name="Testfar",
    race="human",
    sex="male",
    level=12,
    load_room=ROOM_DISTANT_CELL,
    class_name="warrior",
)
# A level-12 human warrior who starts in zone 90, twenty map squares from the caster's zone.
PLUS_TWENTY_VICTIM = Character(
    name="Testplus",
    race="human",
    sex="male",
    level=12,
    load_room=ROOM_PLUS_TWENTY_CELL,
    class_name="warrior",
)
# A level-12 human warrior who starts in zone 91, twenty map squares from the caster's zone on the
# side opposite zone 90.
MINUS_TWENTY_VICTIM = Character(
    name="Testminus",
    race="human",
    sex="male",
    level=12,
    load_room=ROOM_MINUS_TWENTY_CELL,
    class_name="warrior",
)

# The intelligence the implementor gives the caster: the top of wizset's range.
CASTER_INTELLIGENCE = 40
# The intelligence the implementor gives each victim: the bottom of wizset's range.
VICTIM_INTELLIGENCE = 0
# The caster's mage level: a single-class mage's mage level is its overall level, up to 30.
CASTER_MAGE_LEVEL = CASTER.level
# The test world's zone files, which hold each zone's map square.
ZONE_DIRECTORY = Path(__file__).resolve().parents[1] / "world" / "zon"

# What the caster reads when summon's target lookup finds nobody it can see (report_wrong_target,
# interpre.cpp, for a mask with TAR_CHAR_WORLD).
NOBODY_MARKER = "Nobody by that name."
# What the caster reads when the casting delay starts (do_cast, spell_pa.cpp).
CONCENTRATE_MARKER = "You start to concentrate."
# What the caster reads when the concentration roll fails (do_cast): number(0, 100) >= 100 still
# happens once in 101 casts at full knowledge.
LOST_CONCENTRATION_MARKER = "You lost your concentration!"
# What the caster reads when the victim saves or summon refuses the target (spell_summon).
FAILED_MARKER = "You failed."
# What the caster reads when its mana no longer covers a cast (can_cast_spell, spell_pa.cpp).
NO_ENERGY_MARKER = "You can't summon enough energy to cast the spell."
# The start of what the caster reads when do_cast's big_brother check protects the target
# (spell_pa.cpp; a writing or AFK target, big_brother.cpp).
PROTECTED_TARGET_MARKER = "You feel the Gods looking down upon you"
# What the caster reads when the victim has fought another player too recently (spell_summon).
RECENT_PK_MARKER = "Player has engaged in PK too recently to be summoned."
# What the caster reads, in place of the victim's name, when it cannot see the victim in the
# room the victim is leaving: spell_summon's act() runs PERS before the move.
UNSEEN_ARRIVAL_MARKER = "Someone appears in the room."
# What the caster reads when the victim it can see arrives (spell_summon); format with the name.
ARRIVAL_MARKER_FORMAT = "{} appears in the room."
# What the victim reads when it is moved (spell_summon); format with the caster's name.
SUMMONED_MARKER_FORMAT = "{} summons you!"
# What a character reads when the implementor teleports it (do_teleport, act_wiz.cpp).
TELEPORTED_MARKER = "has teleported you!"
# What look prints instead of the room for a character who cannot see (do_look, act_info.cpp).
DARK_ROOM_LOOK = "It is pitch black..."
# The names look prints for the only rooms of zones 12, 90 and 91 (tests/integration/world/wld).
DISTANT_CELL_NAME = "Distant Cell"
PLUS_TWENTY_CELL_NAME = "Plus Twenty Cell"
MINUS_TWENTY_CELL_NAME = "Minus Twenty Cell"
# The save bonus from which new_saves_spell (spell_pa.cpp) returns a save whatever the roll; at its
# negative it returns a failure whatever the roll.
FORCED_SAVE_BONUS = 20
# The stat line just before the affect lines (do_stat_character, act_wiz.cpp): once it arrives,
# the drain collects the rest of the reply.
STAT_LAST_LINE_MARKER = "VUL:"
# Seconds to wait for a cast's outcome: summon's 36 beats at mage level 30 are 18 passes, four
# and a half seconds (CASTING_TIME, spells.h); 15 leaves room for a loaded host or emulated i386.
CAST_TIMEOUT = 15.0
# Seconds to keep reading after an outcome, for text already sent to arrive.
SETTLE_SECONDS = 1.0

# The stat lines new_saves_spell's inputs come from (do_stat_character, act_wiz.cpp).
MAGE_LEVEL_PATTERN = re.compile(r"Class levels: Mag:(\d+) ")
CURRENT_INTELLIGENCE_PATTERN = re.compile(r"Int:\s*\[(-?\d+)/(-?\d+)/(-?\d+)\]")
SPELL_PEN_PATTERN = re.compile(r"Spell_Pen: (-?\d+),")
# A zone file's map line, `<symbol> <x> <y> <level>` (load_zones, zone.cpp).
ZONE_MAP_PATTERN = re.compile(r"^\S (-?\d+) (-?\d+) -?\d+$", re.MULTILINE)


def c_divide(numerator: int, denominator: int) -> int:
    """C's integer division, which truncates toward zero where Python's // floors."""
    return int(numerator / denominator)


def zone_map_square(zone_number: int) -> tuple:
    """The (x, y) map square in the test world's zone file, which spell_summon reads."""
    text = (ZONE_DIRECTORY / f"{zone_number}.zon").read_text()
    match = ZONE_MAP_PATTERN.search(text)
    assert match is not None, f"no map line in zone {zone_number}:\n{text}"
    return int(match.group(1)), int(match.group(2))


def exclusive_or_distance(caster_square: tuple, victim_square: tuple) -> int:
    """The zone distance the legacy pins check: `((ch_x - v_x) ^ 2) + ((ch_y - v_y) ^ 2)`, with `^` as exclusive or.

    Python's `^` on negative integers matches two's-complement C on the game's int.
    """
    return ((caster_square[0] - victim_square[0]) ^ 2) + ((caster_square[1] - victim_square[1]) ^ 2)


def straight_line_distance(caster_square: tuple, victim_square: tuple) -> int:
    """The straight-line distance between two map squares, rounded down."""
    delta_x = caster_square[0] - victim_square[0]
    delta_y = caster_square[1] - victim_square[1]
    return math.isqrt(delta_x * delta_x + delta_y * delta_y)


def save_value(victim_inputs: tuple, save_bonus: int) -> int:
    """new_saves_spell's save_value: get_character_saving_throw(victim) + save_bonus (spell_pa.cpp).

    victim_inputs is save_inputs()'s (mage level, current intelligence, spell penetration); the
    test victims are not hobbits, so the hobbit point is 0.
    """
    mage_level, intelligence = victim_inputs[:2]
    return c_divide(mage_level, 3) + c_divide(intelligence - 8, 4) + 0 + save_bonus


def casting_dc(caster_inputs: tuple) -> int:
    """get_saving_throw_dc (spell_pa.cpp) from save_inputs()'s values for the caster."""
    mage_level, intelligence, spell_pen = caster_inputs
    return 10 + c_divide(mage_level, 3) + c_divide(intelligence - 8, 4) + spell_pen


def set_intelligence(implementor: GameSession, name: str, intelligence: int) -> None:
    """Sets a character's constant intelligence, then runs `restore` so its current value follows.

    The restore also refills the character's hit points, mana and moves.
    """
    reply = implementor.command(f"wizset player {name.lower()} int {intelligence}")
    assert reply.contains(f"set to {intelligence}."), reply.text
    restored = implementor.command(f"restore {name.lower()}")
    assert restored.contains("Done."), restored.text


def save_inputs(implementor: GameSession, name: str) -> tuple:
    """The mage level, current intelligence and spell penetration stat shows for a character."""
    implementor.send_line(f"stat player {name.lower()}")
    text = implementor.expect([STAT_LAST_LINE_MARKER]) + implementor.drain(SETTLE_SECONDS)
    mage_level = MAGE_LEVEL_PATTERN.search(text)
    intelligence = CURRENT_INTELLIGENCE_PATTERN.search(text)
    spell_pen = SPELL_PEN_PATTERN.search(text)
    assert mage_level and intelligence and spell_pen, f"stat lines missing for {name}:\n{text}"
    return int(mage_level.group(1)), int(intelligence.group(1)), int(spell_pen.group(1))


def cast_summon(caster: GameSession, target_name: str) -> str:
    """Casts summon at target_name (as the game prints it); returns what the caster read after.

    A lost concentration is the one roll left that can stop the cast, so the cast is repeated
    after it. Each loss costs mana, and nothing regenerates mana between requests under
    affect-hours, which every test here uses, so the repeats end at NO_ENERGY_MARKER at the latest.
    """
    outcomes = [
        NOBODY_MARKER,
        FAILED_MARKER,
        LOST_CONCENTRATION_MARKER,
        NO_ENERGY_MARKER,
        PROTECTED_TARGET_MARKER,
        RECENT_PK_MARKER,
        UNSEEN_ARRIVAL_MARKER,
        ARRIVAL_MARKER_FORMAT.format(target_name),
    ]
    while True:
        caster.send_line(f"cast 'summon' {target_name.lower()}")
        reply = caster.expect(outcomes, timeout=CAST_TIMEOUT) + caster.drain(SETTLE_SECONDS)
        if LOST_CONCENTRATION_MARKER not in reply:
            return reply


def setup_caster_and_victim(implementor: GameSession, victim: Character) -> tuple:
    """Gives the caster and the victim their intelligences and returns both save inputs.

    Holylight lets the implementor stat a character standing in the dark cell.
    """
    holylight = implementor.command("set holylight on")
    assert holylight.contains("Holylight mode is now on."), holylight.text
    set_intelligence(implementor, CASTER.name, CASTER_INTELLIGENCE)
    set_intelligence(implementor, victim.name, VICTIM_INTELLIGENCE)
    return save_inputs(implementor, CASTER.name), save_inputs(implementor, victim.name)


def setup_another_victim(implementor: GameSession, victim: Character) -> tuple:
    """Gives a further victim the victims' intelligence and returns its save inputs."""
    set_intelligence(implementor, victim.name, VICTIM_INTELLIGENCE)
    return save_inputs(implementor, victim.name)


def refill_caster(implementor: GameSession) -> None:
    """Restores the caster's mana for another cast.

    Its abilities are already at their base after setup, so the save inputs read then still hold.
    """
    restored = implementor.command(f"restore {CASTER.name.lower()}")
    assert restored.contains("Done."), restored.text


@pytest.mark.xfail(strict=True, raises=AssertionError, reason="summon reaches a player standing in a dark room")
@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, CASTER, DARK_VICTIM)
def test_a_mage_cannot_summon_a_player_standing_in_a_dark_room(server) -> None:
    """Legacy pin: summon cannot reach a player in a DARK room, though it reaches a lit one.

    Records today's behaviour. Summon's target mask (6, skills[] in consts.cpp) lacks
    TAR_DARK_OK, so the caster's world lookup (get_char_vis) needs CAN_SEE, and a human caster
    without infravision or holylight cannot see into an unlit DARK room. The cast stops at the
    lookup, before any roll. The interpreter's target_check_one (interpre.cpp) repeats the same
    sight test when the casting delay ends, so reaching dark rooms needs both checks changed.
    The control moves the same victim to a lit room of the same zone.
    The one draw left that can change the outcome is the concentration roll in do_cast, which
    cast_summon repeats; the affect-hours schedule keeps everything else from drawing. When
    summon is changed to reach dark rooms, this test becomes a strict xfail.
    """
    implementor = server.login(IMPLEMENTOR.name)
    caster = server.login(CASTER.name)
    victim = server.login(DARK_VICTIM.name)

    assert victim.command("look").room_name() == DARK_ROOM_LOOK
    caster_inputs, victim_inputs = setup_caster_and_victim(implementor, DARK_VICTIM)

    # Dark case, Testlost in the dark cell (1133, DARK, zone 11): no margin applies, because
    # target_from_word (interpre.cpp) finds no target the caster can see and do_cast returns
    # with report_wrong_target before the delay, the concentration roll or new_saves_spell.
    dark_reply = cast_summon(caster, DARK_VICTIM.name)
    assert NOBODY_MARKER in dark_reply, dark_reply
    assert CONCENTRATE_MARKER not in dark_reply, dark_reply
    assert victim.command("look").room_name() == DARK_ROOM_LOOK

    implementor.command(f"teleport {DARK_VICTIM.name.lower()} {world.ROOM_ARENA_WEST}")
    victim.expect([TELEPORTED_MARKER])
    victim.drain(SETTLE_SECONDS)

    # Control case, Testlost in Arena West (1130, lit, zone 11), caster in Arena Centre (zone 11):
    #   dist       = ((ch_x - v_x) ^ 2) + ((ch_y - v_y) ^ 2)        (spell_summon, mage.cpp)
    #              = ((0 - 0) ^ 2) + ((0 - 0) ^ 2)                  (zone 11's map square (0, 0))
    #              = 2 + 2 = 4                                      (^ is exclusive or: 0 ^ 2 = 2)
    #   save_bonus = dist = 4                (the NPC level bonus does not apply to a player)
    #   save_value = get_character_saving_throw(victim) + save_bonus    (spell_pa.cpp)
    #              = Mag / 3 + (Int - 8) / 4 + 0 + save_bonus           (0: not a hobbit)
    #              = 3 / 3   + (0 - 8) / 4   + 0 + 4                    (Mag:3, Int:[0/0/0])
    #              = 1 + -2 + 0 + 4 = 3
    #   casting_dc = 10 + Mag / 3 + (Int - 8) / 4 + Spell_Pen           (get_saving_throw_dc)
    #              = 10 + 30 / 3  + (40 - 8) / 4  + 0
    #                (Mag:30, Int:[40/40/40], Spell_Pen: 0)
    #              = 10 + 10 + 8 + 0 = 28
    #   saved      = roll + save_value > casting_dc, roll = number(1, 20)  (new_saves_spell)
    #              = at most 20 + 3 = 23, never above 28: never saves. A save_bonus of 0
    #                (squared distances) gives at most 19, so this case holds whatever the
    #                arithmetic.
    assert caster_inputs == (CASTER_MAGE_LEVEL, CASTER_INTELLIGENCE, 0), caster_inputs
    assert victim_inputs == (3, VICTIM_INTELLIGENCE, 0), victim_inputs
    caster_mage_level, caster_intelligence, caster_spell_pen = caster_inputs
    victim_mage_level, victim_intelligence = victim_inputs[:2]
    caster_x, caster_y = zone_map_square(11)
    victim_x, victim_y = zone_map_square(11)
    distance = ((caster_x - victim_x) ^ 2) + ((caster_y - victim_y) ^ 2)
    save_value = c_divide(victim_mage_level, 3) + c_divide(victim_intelligence - 8, 4) + 0 + distance
    casting_dc = 10 + c_divide(caster_mage_level, 3) + c_divide(caster_intelligence - 8, 4) + caster_spell_pen
    assert ((caster_x, caster_y), (victim_x, victim_y)) == ((0, 0), (0, 0))
    assert (distance, save_value, casting_dc) == (4, 3, 28)
    assert 20 + save_value <= casting_dc

    lit_reply = cast_summon(caster, DARK_VICTIM.name)
    assert ARRIVAL_MARKER_FORMAT.format(DARK_VICTIM.name) in lit_reply, lit_reply
    summoned_marker = SUMMONED_MARKER_FORMAT.format(CASTER.name)
    summoned = victim.expect([summoned_marker]) + victim.drain(SETTLE_SECONDS)
    assert "Arena Centre" in summoned, summoned


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, CASTER, DARK_VICTIM)
def test_a_mage_summons_a_player_standing_in_a_dark_room(server) -> None:
    """New-behaviour pin: summon reaches a player standing in a DARK room.

    Summon's targets carry TAR_DARK_OK, so neither the caster's lookup when the cast starts nor
    the interpreter's re-check when the casting delay ends needs to see into the room. The caster
    still cannot see the victim in the room it is leaving, so it reads UNSEEN_ARRIVAL_MARKER; the
    victim reads the caster's name and arrives in the lit Arena Centre.
    The one draw left that can change the outcome is the concentration roll in do_cast, which
    cast_summon repeats; the affect-hours schedule keeps everything else from drawing.
    """
    implementor = server.login(IMPLEMENTOR.name)
    caster = server.login(CASTER.name)
    victim = server.login(DARK_VICTIM.name)

    assert victim.command("look").room_name() == DARK_ROOM_LOOK
    caster_inputs, victim_inputs = setup_caster_and_victim(implementor, DARK_VICTIM)

    # Dark case, Testlost in the dark cell (1133, DARK, zone 11), caster in Arena Centre (zone 11):
    #   dist       = (int)sqrt(delta_x * delta_x + delta_y * delta_y)   (spell_summon, mage.cpp)
    #              = (int)sqrt((0 - 0) * (0 - 0) + (0 - 0) * (0 - 0))  (zone 11's map square (0, 0))
    #              = (int)sqrt(0) = 0
    #   save_bonus = dist = 0                (the NPC level bonus does not apply to a player)
    #   save_value = get_character_saving_throw(victim) + save_bonus    (spell_pa.cpp)
    #              = Mag / 3 + (Int - 8) / 4 + 0 + save_bonus           (0: not a hobbit)
    #              = 3 / 3   + (0 - 8) / 4   + 0 + 0                    (Mag:3, Int:[0/0/0])
    #              = 1 + -2 + 0 + 0 = -1
    #   casting_dc = 10 + Mag / 3 + (Int - 8) / 4 + Spell_Pen           (get_saving_throw_dc)
    #              = 10 + 30 / 3  + (40 - 8) / 4  + 0
    #                (Mag:30, Int:[40/40/40], Spell_Pen: 0)
    #              = 10 + 10 + 8 + 0 = 28
    #   saved      = roll + save_value > casting_dc, roll = number(1, 20)  (new_saves_spell)
    #              = at most 20 + -1 = 19, never above 28: never saves.
    assert caster_inputs == (CASTER_MAGE_LEVEL, CASTER_INTELLIGENCE, 0), caster_inputs
    assert victim_inputs == (3, VICTIM_INTELLIGENCE, 0), victim_inputs
    caster_square = zone_map_square(11)
    assert caster_square == (0, 0)
    distance = straight_line_distance(caster_square, caster_square)
    victim_save_value = save_value(victim_inputs, distance)
    caster_dc = casting_dc(caster_inputs)
    assert (distance, victim_save_value, caster_dc) == (0, -1, 28)
    assert 20 + victim_save_value <= caster_dc

    reply = cast_summon(caster, DARK_VICTIM.name)
    assert UNSEEN_ARRIVAL_MARKER in reply, reply
    summoned_marker = SUMMONED_MARKER_FORMAT.format(CASTER.name)
    summoned = victim.expect([summoned_marker]) + victim.drain(SETTLE_SECONDS)
    assert "Arena Centre" in summoned, summoned


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, CASTER, DISTANT_VICTIM)
def test_a_player_five_map_squares_away_is_summoned(server) -> None:
    """Control: a weak player five map squares away is summoned.

    spell_summon (mage.cpp) adds a bonus for the distance between the zones' map squares to the
    victim's save. From zone 11 at (0, 0) to zone 12 at (3, 4) the exclusive-or arithmetic gives
    -3 and the straight-line distance gives 5. With this caster and victim neither bonus lets any
    roll pass the caster's DC, so the summon lands under either arithmetic.
    The one draw left that can change the outcome is the concentration roll in do_cast, which
    cast_summon repeats; the affect-hours schedule keeps everything else from drawing.
    """
    implementor = server.login(IMPLEMENTOR.name)
    caster = server.login(CASTER.name)
    victim = server.login(DISTANT_VICTIM.name)

    assert victim.command("look").room_name() == DISTANT_CELL_NAME
    caster_inputs, victim_inputs = setup_caster_and_victim(implementor, DISTANT_VICTIM)

    # Distant case, caster in Arena Centre (zone 11), Testfar in the Distant Cell (zone 12):
    #   With exclusive or:
    #   dist       = ((ch_x - v_x) ^ 2) + ((ch_y - v_y) ^ 2)
    #              = ((0 - 3) ^ 2) + ((0 - 4) ^ 2)                  (zone 11 (0, 0), zone 12 (3, 4))
    #              = (-3 ^ 2) + (-4 ^ 2) = -1 + -2 = -3
    #                (^ is exclusive or: it flips bit 1 of each difference)
    #   With the straight line:
    #   dist       = (int)sqrt(delta_x * delta_x + delta_y * delta_y)
    #              = (int)sqrt((0 - 3) * (0 - 3) + (0 - 4) * (0 - 4)) = (int)sqrt(25) = 5
    #   save_bonus = dist                    (the NPC level bonus does not apply to a player)
    #   save_value = get_character_saving_throw(victim) + save_bonus    (spell_pa.cpp)
    #              = Mag / 3 + (Int - 8) / 4 + 0 + save_bonus           (0: not a hobbit)
    #              = 3 / 3   + (0 - 8) / 4   + 0 + save_bonus           (Mag:3, Int:[0/0/0])
    #              = 1 + -2 + 0 + -3 = -4 with exclusive or, 1 + -2 + 0 + 5 = 4 in a straight line
    #   casting_dc = 10 + Mag / 3 + (Int - 8) / 4 + Spell_Pen           (get_saving_throw_dc)
    #              = 10 + 30 / 3  + (40 - 8) / 4  + 0
    #                (Mag:30, Int:[40/40/40], Spell_Pen: 0)
    #              = 10 + 10 + 8 + 0 = 28
    #   saved      = roll + save_value > casting_dc, roll = number(1, 20)  (new_saves_spell)
    #              = at most 20 + -4 = 16, or 20 + 4 = 24, never above 28: never saves. Both -3
    #                and 5 are inside (-20, 20), so new_saves_spell returns that result rather
    #                than a forced one.
    assert caster_inputs == (CASTER_MAGE_LEVEL, CASTER_INTELLIGENCE, 0), caster_inputs
    assert victim_inputs == (3, VICTIM_INTELLIGENCE, 0), victim_inputs
    caster_square = zone_map_square(11)
    victim_square = zone_map_square(12)
    assert (caster_square, victim_square) == ((0, 0), (3, 4))
    exclusive_or = exclusive_or_distance(caster_square, victim_square)
    straight_line = straight_line_distance(caster_square, victim_square)
    assert (exclusive_or, straight_line) == (-3, 5)
    assert casting_dc(caster_inputs) == 28
    for distance in (exclusive_or, straight_line):
        assert -FORCED_SAVE_BONUS < distance < FORCED_SAVE_BONUS
        assert 20 + save_value(victim_inputs, distance) <= casting_dc(caster_inputs)

    reply = cast_summon(caster, DISTANT_VICTIM.name)
    assert ARRIVAL_MARKER_FORMAT.format(DISTANT_VICTIM.name) in reply, reply
    summoned_marker = SUMMONED_MARKER_FORMAT.format(CASTER.name)
    summoned = victim.expect([summoned_marker]) + victim.drain(SETTLE_SECONDS)
    assert "Arena Centre" in summoned, summoned


@pytest.mark.xfail(
    strict=True,
    raises=AssertionError,
    reason="summon's distance bonus is the straight-line distance, the same in both directions",
)
@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, CASTER, PLUS_TWENTY_VICTIM, MINUS_TWENTY_VICTIM)
def test_summon_reaches_a_player_twenty_squares_away_on_one_side_but_never_the_same_distance_on_the_other(
    server,
) -> None:
    """Legacy pin: summon's exclusive-or distance makes twenty squares certain one way, impossible the other.

    Records today's behaviour. spell_summon (mage.cpp) adds `((ch_x - v_x) ^ 2) + ((ch_y - v_y) ^ 2)`
    to the victim's save, where `^` is C's exclusive or. Zone 90 at (20, 0) and zone 91 at
    (-20, 0) are both twenty squares from zone 11 at (0, 0), but the bonus is -16 for zone 90, so
    a weak victim there never saves, and 24 for zone 91, past new_saves_spell's forced save, so
    that victim always saves.
    The one draw left that can change the outcome is the concentration roll in do_cast, which
    cast_summon repeats; the affect-hours schedule keeps everything else from drawing. When the
    distance arithmetic changes, this test becomes a strict xfail.
    """
    implementor = server.login(IMPLEMENTOR.name)
    caster = server.login(CASTER.name)
    plus_victim = server.login(PLUS_TWENTY_VICTIM.name)
    minus_victim = server.login(MINUS_TWENTY_VICTIM.name)

    assert plus_victim.command("look").room_name() == PLUS_TWENTY_CELL_NAME
    assert minus_victim.command("look").room_name() == MINUS_TWENTY_CELL_NAME
    caster_inputs, plus_inputs = setup_caster_and_victim(implementor, PLUS_TWENTY_VICTIM)
    minus_inputs = setup_another_victim(implementor, MINUS_TWENTY_VICTIM)

    # Zone 90 case, caster in Arena Centre (zone 11), Testplus in the Plus Twenty Cell (zone 90):
    #   dist       = ((ch_x - v_x) ^ 2) + ((ch_y - v_y) ^ 2)        (spell_summon, mage.cpp)
    #              = ((0 - 20) ^ 2) + ((0 - 0) ^ 2)                 (zone 11 (0, 0), zone 90 (20, 0))
    #              = (-20 ^ 2) + (0 ^ 2) = -18 + 2 = -16
    #                (^ is exclusive or: it flips bit 1 of each difference)
    #   save_bonus = dist = -16              (the NPC level bonus does not apply to a player)
    #   save_value = get_character_saving_throw(victim) + save_bonus    (spell_pa.cpp)
    #              = Mag / 3 + (Int - 8) / 4 + 0 + save_bonus           (0: not a hobbit)
    #              = 3 / 3   + (0 - 8) / 4   + 0 + -16                  (Mag:3, Int:[0/0/0])
    #              = 1 + -2 + 0 + -16 = -17
    #   casting_dc = 10 + Mag / 3 + (Int - 8) / 4 + Spell_Pen           (get_saving_throw_dc)
    #              = 10 + 30 / 3  + (40 - 8) / 4  + 0
    #                (Mag:30, Int:[40/40/40], Spell_Pen: 0)
    #              = 10 + 10 + 8 + 0 = 28
    #   saved      = roll + save_value > casting_dc, roll = number(1, 20)  (new_saves_spell)
    #              = at most 20 + -17 = 3, never above 28: never saves; -16 is inside (-20, 20),
    #                so new_saves_spell returns that result rather than a forced one.
    # Zone 91 case, Testminus in the Minus Twenty Cell (zone 91):
    #   dist       = ((0 - -20) ^ 2) + ((0 - 0) ^ 2)                (zone 11 (0, 0), zone 91 (-20, 0))
    #              = (20 ^ 2) + (0 ^ 2) = 22 + 2 = 24
    #   save_bonus = dist = 24, at least 20: new_saves_spell returns a save whatever the roll
    #                (spell_pa.cpp), so this victim always saves.
    assert caster_inputs == (CASTER_MAGE_LEVEL, CASTER_INTELLIGENCE, 0), caster_inputs
    assert plus_inputs == (3, VICTIM_INTELLIGENCE, 0), plus_inputs
    assert minus_inputs == (3, VICTIM_INTELLIGENCE, 0), minus_inputs
    caster_square = zone_map_square(11)
    plus_square = zone_map_square(90)
    minus_square = zone_map_square(91)
    assert (caster_square, plus_square, minus_square) == ((0, 0), (20, 0), (-20, 0))
    plus_distance = exclusive_or_distance(caster_square, plus_square)
    minus_distance = exclusive_or_distance(caster_square, minus_square)
    assert (plus_distance, minus_distance) == (-16, 24)
    assert (save_value(plus_inputs, plus_distance), casting_dc(caster_inputs)) == (-17, 28)
    assert -FORCED_SAVE_BONUS < plus_distance < FORCED_SAVE_BONUS
    assert 20 + save_value(plus_inputs, plus_distance) <= casting_dc(caster_inputs)
    assert minus_distance >= FORCED_SAVE_BONUS

    plus_reply = cast_summon(caster, PLUS_TWENTY_VICTIM.name)
    assert ARRIVAL_MARKER_FORMAT.format(PLUS_TWENTY_VICTIM.name) in plus_reply, plus_reply
    summoned_marker = SUMMONED_MARKER_FORMAT.format(CASTER.name)
    summoned = plus_victim.expect([summoned_marker]) + plus_victim.drain(SETTLE_SECONDS)
    assert "Arena Centre" in summoned, summoned

    refill_caster(implementor)
    minus_reply = cast_summon(caster, MINUS_TWENTY_VICTIM.name)
    assert FAILED_MARKER in minus_reply, minus_reply
    assert minus_victim.command("look").room_name() == MINUS_TWENTY_CELL_NAME


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, CASTER, PLUS_TWENTY_VICTIM, MINUS_TWENTY_VICTIM)
def test_a_player_twenty_map_squares_away_always_resists_in_either_direction(server) -> None:
    """New-behaviour pin: a summon across twenty map squares always fails, whichever side.

    spell_summon (mage.cpp) adds the straight-line distance between the two zones' map squares,
    rounded down, to the victim's save: 20 from zone 11 at (0, 0) to zone 90 at (20, 0), and 20
    to zone 91 at (-20, 0). new_saves_spell (spell_pa.cpp) returns a save whatever the roll once
    the bonus reaches FORCED_SAVE_BONUS, so the caster reads "You failed." both times and neither
    victim moves. The caster and the victims are set up as in the legacy twenty-square pin.
    The one draw left that can change the outcome is the concentration roll in do_cast, which
    cast_summon repeats; the affect-hours schedule keeps everything else from drawing.
    """
    implementor = server.login(IMPLEMENTOR.name)
    caster = server.login(CASTER.name)
    plus_victim = server.login(PLUS_TWENTY_VICTIM.name)
    minus_victim = server.login(MINUS_TWENTY_VICTIM.name)

    assert plus_victim.command("look").room_name() == PLUS_TWENTY_CELL_NAME
    assert minus_victim.command("look").room_name() == MINUS_TWENTY_CELL_NAME
    caster_inputs, plus_inputs = setup_caster_and_victim(implementor, PLUS_TWENTY_VICTIM)
    minus_inputs = setup_another_victim(implementor, MINUS_TWENTY_VICTIM)

    # Zone 90 case, caster in Arena Centre (zone 11), Testplus in the Plus Twenty Cell (zone 90):
    #   dist       = (int)sqrt(delta_x * delta_x + delta_y * delta_y)      (spell_summon, mage.cpp)
    #              = (int)sqrt((0 - 20) * (0 - 20) + (0 - 0) * (0 - 0))  (zone 11 (0, 0), zone 90 (20, 0))
    #              = (int)sqrt(400) = 20
    # Zone 91 case, Testminus in the Minus Twenty Cell (zone 91):
    #   dist       = (int)sqrt((0 - -20) * (0 - -20) + (0 - 0) * (0 - 0)) (zone 91 (-20, 0))
    #              = (int)sqrt(400) = 20
    # Both cases:
    #   save_bonus = dist = 20               (the NPC level bonus does not apply to a player)
    #   saved      = save_bonus >= 20: true  (new_saves_spell, spell_pa.cpp: it returns a save for
    #                any save_bonus of 20 or more, whatever the roll; roll + save_value against
    #                casting_dc, at most 20 + (1 + -2 + 0 + 20) = 39 against 28, is not consulted)
    assert caster_inputs == (CASTER_MAGE_LEVEL, CASTER_INTELLIGENCE, 0), caster_inputs
    assert plus_inputs == (3, VICTIM_INTELLIGENCE, 0), plus_inputs
    assert minus_inputs == (3, VICTIM_INTELLIGENCE, 0), minus_inputs
    caster_square = zone_map_square(11)
    plus_square = zone_map_square(90)
    minus_square = zone_map_square(91)
    assert (caster_square, plus_square, minus_square) == ((0, 0), (20, 0), (-20, 0))
    plus_distance = straight_line_distance(caster_square, plus_square)
    minus_distance = straight_line_distance(caster_square, minus_square)
    assert (plus_distance, minus_distance) == (20, 20)
    assert min(plus_distance, minus_distance) >= FORCED_SAVE_BONUS

    plus_reply = cast_summon(caster, PLUS_TWENTY_VICTIM.name)
    assert FAILED_MARKER in plus_reply, plus_reply
    assert plus_victim.command("look").room_name() == PLUS_TWENTY_CELL_NAME

    refill_caster(implementor)
    minus_reply = cast_summon(caster, MINUS_TWENTY_VICTIM.name)
    assert FAILED_MARKER in minus_reply, minus_reply
    assert minus_victim.command("look").room_name() == MINUS_TWENTY_CELL_NAME
