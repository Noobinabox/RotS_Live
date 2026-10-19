"""Object versions and recorded enchants: enchant weapon records its enchant on the item, and a
version bump refreshes a player's saved copy at the next login, keeping the enchant only while
the new prototype can still take it."""

from __future__ import annotations

import re
import time
from pathlib import Path

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.object_versions]

# A level-30 human mystic in the implementors' room who knows enchant weapon fully: a mortal
# caster, and the mortal who rents at the inn. Enchant weapon is a level-20 mystic spell (skills[]
# in consts.cpp).
MORTAL_MYSTIC = Character(
    name="Testmystic",
    race="human",
    sex="female",
    level=30,
    load_room=world.ROOM_IMMORTAL_START,
    class_name="mystic",
    skills=(("enchant_weapon", 100),),
)
# A level-91 mystic who owns the enchanted weapon across a quit. It is an immortal because only
# an immortal's quit saves what it carries (do_quit, act_othe.cpp): a mortal's possessions drop
# to the ground, and a mortal keeps them only by renting at the inn.
OWNER = Character(
    name="Testowner",
    race="human",
    sex="male",
    level=91,
    load_room=world.ROOM_IMMORTAL_START,
    class_name="mystic",
    skills=(("enchant_weapon", 100),),
)

# The plain sword's virtual number (tests/integration/world/obj/16.obj): a weapon without the
# MAGIC flag or affects of its own, so enchant weapon takes on it (object_takes_enchant).
PLAIN_SWORD_VNUM = 1601
# The keyword the sword answers to, and its short description.
SWORD_KEYWORD = "sword"
SWORD_SHORT_DESCRIPTION = "a plain sword"
# Spirit the implementor gives each caster: enchant weapon costs 50 and a lost concentration 25
# (do_cast, spell_pa.cpp), and nothing restores spirit between requests under affect-hours.
CASTER_SPIRIT = 500
# Alignments wizset gives a caster, at the ends of its range and the middle (IS_GOOD at 100 or
# more, IS_EVIL at -100 or less, utils.h).
GOOD_ALIGNMENT = 1000
EVIL_ALIGNMENT = -1000
NEUTRAL_ALIGNMENT = 0
# The affect the builder gives the prototype in the object editor's field 19 so the sword can no
# longer take an enchant: location 1 (STR), modifier 1.
STRENGTH_AFFECT_ANSWER = "(1 1)"

# What the caster reads when the spell goes off (do_cast).
CAST_DONE_MARKER = "Ok."
# What the caster reads when the concentration roll fails (do_cast): number(0, 100) >= 100 still
# happens once in 101 casts at full knowledge.
LOST_CONCENTRATION_MARKER = "You lost your concentration!"
# What the caster reads when its spirit no longer covers a cast (can_cast_spell, spell_pa.cpp).
NO_ENERGY_MARKER = "You can't summon enough energy to cast the spell."
# What the caster reads when it does not carry the target (report_wrong_target, interpre.cpp).
NO_TARGET_MARKER = "You don't have that."
# What the caster reads when it does not know the spell (can_cast_spell).
UNKNOWN_SPELL_MARKER = "You don't know this spell."
# What enchant weapon prints for a weapon that has an affect but not the MAGIC flag
# (spell_enchant_weapon, mystic.cpp).
TOO_MUCH_MAGIC_MARKER = "There is too much magic in it already."
# What the caster reads when the enchant takes, by the caster's alignment (spell_enchant_weapon).
GLOW_MARKER_FORMAT = "A plain sword glows {}."
GLOW_FRAGMENT = " glows "
# What the owner reads after the room on entering the game, once per refreshed item
# (Crash_tell_object_refresh, objsave.cpp).
UPDATED_MARKER_FORMAT = "Your item, {}, has been updated."
# What the implementor reads when the load and the give go through (do_load, act_wiz.cpp; give).
CREATED_MARKER = "You create a plain sword."
GIVEN_MARKER_FORMAT = "You give a plain sword to {}."

# The stat obj lines this file pins (do_stat_object, act_wiz.cpp); the last line of the reply is
# the affections line.
STAT_LAST_LINE_MARKER = "Affections:"
NO_SUCH_OBJECT_MARKER = "No such object around."
UNVERSIONED_LINE = "Version: prototype none, this copy none"
BUMPED_BEFORE_LOGIN_LINE = "Version: prototype 1 (on), this copy none"
REFRESHED_LINE = "Version: prototype 1 (on), this copy 1"
NO_ENCHANTS_LINE = "Enchants: none"
ENCHANT_LINE_FORMAT = "Enchants: ob +6 ({})"
ENCHANT_AFFECTIONS_LINE = "Affections: +6 to OB"
STRENGTH_AFFECTIONS_LINE = "Affections: +1 to STR"

# The object editor's replies (shapeobj.cpp) and its prompt, "Object: <vnum>>" (consts.cpp).
EDITOR_PROMPT = f"Object: {PLAIN_SWORD_VNUM}>"
EDITOR_LOADED_MARKER = "Causing the eternal order to snicker, you load an object."
EDITOR_NO_FILE_MARKER = "could not open that file."
EDITOR_NO_OBJECT_MARKER = "no object here."
VERSION_PROMPT = "Enter VERSION: b, blank keeps it [none]:"
VERSION_BUMPED_MARKER = "Version is now 1 (on)."
VERSION_UNCHANGED_MARKER = "Version unchanged."
AFFECTS_PROMPT = "Current: (0 0) (0 0)"
AFFECTS_DROPPED_MARKER = "No affections were set. Dropped."
IMPLEMENTED_MARKER = "Object implemented"
NOTHING_TO_IMPLEMENT_MARKER = "You have nothing to implement"
NOT_AUTHORIZED_MARKER = "You're not authorized to do this."
RELEASED_MARKER = "You released an object and stopped shaping."

# What the mortal reads when the implementor teleports it (do_teleport, act_wiz.cpp), and what
# the implementor reads when the name matches no one.
TELEPORTED_MARKER = f"{IMPLEMENTOR.name} has teleported you!"
NO_ONE_MARKER = "No-one by that name around."
# The inn's name and its innkeeper's long description (tests/integration/world: wld/16.wld,
# mob/11.mob), which a character in the inn reads in the room.
INN_ROOM_NAME = "Harness Inn"
INNKEEPER_IN_ROOM_LINE = "The harness innkeeper stands behind the counter, ready to rent you a room."
# What the innkeeper's receptionist special answers (gen_receptionist and Crash_offer_rent,
# objsave.cpp): the rent, the refusals the code can reach here, and the offer's two lines.
RENTED_MARKER = "The harness innkeeper stores your belongings and helps you into your private chamber."
NOTHING_TO_STORE_MARKER = "The harness innkeeper tells you, 'But you are not carrying anything!  Just quit!'"
CANNOT_SEE_RENTER_MARKER = "The harness innkeeper says, 'I don't deal with people I can't see!'"
OFFER_QUOTE_MARKER = "The harness innkeeper tells you, 'It will cost you 6 silver and 88 copper for the first day.'"
LIFETIME_OF_RENT_MARKER = "The harness innkeeper tells you, 'You have enough gold for a lifetime of rent.'"
# What rent and offer answer with no receptionist in the room (do_rent, objsave.cpp; do_not_here,
# act_othe.cpp).
FIELD_RENT_DISABLED_MARKER = "Field-rent is disabled. You have to go to an inn now."
NOT_HERE_MARKER = "Sorry, but you can't do that here!"
# The character menu's prompt (show_character_menu_impl, interpre.cpp): rent ends the character's
# play there, leaving the connection open.
CHARACTER_MENU_PROMPT = "Make your choice:"

# The server log line each refresh writes (Crash_refresh_object, objsave.cpp); format with the
# owner's idnum and the enchant note.
REFRESH_LOG_FORMAT = "Object version refresh: obj 1601, 0 -> 1, player idnum {}, enchant {}"
# The stat player line with the character's idnum (do_stat_character, act_wiz.cpp).
IDNUM_PATTERN = re.compile(r"IDNum: \[\s*(\d+)\]")
# The stat player line just before the affect lines: once it arrives, the drain collects the rest.
STAT_PLAYER_LAST_LINE_MARKER = "VUL:"

# Seconds to wait for a cast's outcome: enchant weapon's 72 beats at mystic level 30 are 36
# (CASTING_TIME, spells.h), four and a half seconds; 15 leaves room for a loaded host.
CAST_TIMEOUT = 15.0
# How much of the owner's login text a failed assertion quotes.
QUOTED_TEXT_LENGTH = 600
# Seconds to keep reading after an outcome, for text already sent to arrive.
SETTLE_SECONDS = 1.0
# Seconds to wait for a line to reach game.log: the Docker launcher relays the server's stderr.
LOG_TIMEOUT = 15.0
# Seconds between two reads of game.log while waiting for a line.
LOG_POLL_SECONDS = 0.25


def give_plain_sword(implementor: GameSession, receiver: Character) -> None:
    """Loads a plain sword and hands it to receiver, who stands in the implementor's room."""
    created = implementor.command(f"load obj {PLAIN_SWORD_VNUM}")
    assert created.contains(CREATED_MARKER), created.text
    given = implementor.command(f"give {SWORD_KEYWORD} {receiver.name.lower()}")
    assert given.contains(GIVEN_MARKER_FORMAT.format(receiver.name)), given.text


def wizset(implementor: GameSession, name: str, field: str, value: int) -> None:
    """Sets a character's field and asserts wizset's reply."""
    reply = implementor.command(f"wizset player {name.lower()} {field} {value}")
    assert reply.contains(f"{name}'s {field} set to {value}."), reply.text


def prepare_caster(implementor: GameSession, caster: Character, alignment: int) -> None:
    """Gives the caster its alignment and enough spirit for the casts and their repeats."""
    wizset(implementor, caster.name, "alignment", alignment)
    wizset(implementor, caster.name, "spirit", CASTER_SPIRIT)


def cast_enchant_weapon(caster: GameSession) -> str:
    """Casts enchant weapon on the sword; returns what the caster read once the spell went off.

    A lost concentration is the one roll that can stop the cast, so the cast is repeated after
    it. Each loss costs spirit and nothing restores it between requests under affect-hours, so
    the repeats end at NO_ENERGY_MARKER at the latest.
    """
    outcomes = [CAST_DONE_MARKER, LOST_CONCENTRATION_MARKER, NO_ENERGY_MARKER, NO_TARGET_MARKER, UNKNOWN_SPELL_MARKER]
    while True:
        caster.send_line(f"cast 'enchant weapon' {SWORD_KEYWORD}")
        reply = caster.expect(outcomes, timeout=CAST_TIMEOUT) + caster.drain(SETTLE_SECONDS)
        if LOST_CONCENTRATION_MARKER not in reply:
            return reply


def stat_sword(implementor: GameSession) -> list:
    """The lines of the implementor's stat obj on the sword, stripped."""
    implementor.send_line(f"stat obj {SWORD_KEYWORD}")
    text = implementor.expect([STAT_LAST_LINE_MARKER, NO_SUCH_OBJECT_MARKER]) + implementor.drain(SETTLE_SECONDS)
    assert NO_SUCH_OBJECT_MARKER not in text, text
    return [line.strip() for line in text.splitlines()]


def player_idnum(implementor: GameSession, name: str) -> int:
    """The character's idnum, from stat player."""
    implementor.send_line(f"stat player {name.lower()}")
    text = implementor.expect([STAT_PLAYER_LAST_LINE_MARKER]) + implementor.drain(SETTLE_SECONDS)
    match = IDNUM_PATTERN.search(text)
    assert match is not None, f"no IDNum in stat player {name}:\n{text}"
    return int(match.group(1))


def editor_step(implementor: GameSession, line: str, expected_marker: str, other_outcomes: list) -> str:
    """Sends one line to the object editor and returns what it read, asserting the expected reply."""
    implementor.send_line(line)
    reply = implementor.expect([expected_marker, *other_outcomes]) + implementor.drain(SETTLE_SECONDS)
    assert expected_marker in reply, f"{line!r}:\n{reply}"
    return reply


def shape_sword_prototype(implementor: GameSession, give_strength_affect: bool) -> None:
    """Bumps the sword prototype's version in the object editor and implements it; optionally
    gives the prototype a strength affect first. Always ends with /free."""
    editor_step(implementor, f"shape object {PLAIN_SWORD_VNUM}", EDITOR_LOADED_MARKER, [EDITOR_NO_FILE_MARKER, EDITOR_NO_OBJECT_MARKER])
    try:
        if give_strength_affect:
            editor_step(implementor, "/19", AFFECTS_PROMPT, [])
            editor_step(implementor, STRENGTH_AFFECT_ANSWER, EDITOR_PROMPT, [AFFECTS_DROPPED_MARKER])
        editor_step(implementor, "/22", VERSION_PROMPT, [])
        editor_step(implementor, "b", VERSION_BUMPED_MARKER, [VERSION_UNCHANGED_MARKER])
        editor_step(implementor, "/implement", IMPLEMENTED_MARKER, [NOTHING_TO_IMPLEMENT_MARKER, NOT_AUTHORIZED_MARKER])
    finally:
        editor_step(implementor, "/free", RELEASED_MARKER, [])


def quit_and_log_in_again(server, session: GameSession, character: Character) -> GameSession:
    """Quits the character and logs it in again on a new connection; returns the new session."""
    session.quit()
    return server.login(character.name)


def send_to_inn(implementor: GameSession, mortal: GameSession, character: Character) -> None:
    """Teleports the mortal into the inn and asserts it arrived where the innkeeper stands.

    The mortal types nothing in the inn before rent or offer: the receptionist special draws for
    every other command typed there, and on a 0 the innkeeper performs a social (gen_receptionist).
    """
    teleport = implementor.command(f"teleport {character.name.lower()} {world.ROOM_INN}")
    assert not teleport.contains(NO_ONE_MARKER), teleport.text
    arrival = mortal.expect([TELEPORTED_MARKER]) + mortal.drain(SETTLE_SECONDS)
    assert INN_ROOM_NAME in arrival, arrival
    assert INNKEEPER_IN_ROOM_LINE in arrival, arrival


def rent_and_log_in_again(server, mortal: GameSession, character: Character) -> GameSession:
    """Rents at the innkeeper, asserting it stores the mortal's belongings and returns the connection
    to the character menu; closes that connection and logs the character in again on a new one,
    which it returns."""
    mortal.send_line("rent")
    outcomes = [CHARACTER_MENU_PROMPT, NOTHING_TO_STORE_MARKER, CANNOT_SEE_RENTER_MARKER, FIELD_RENT_DISABLED_MARKER]
    reply = mortal.expect(outcomes) + mortal.drain(SETTLE_SECONDS)
    assert RENTED_MARKER in reply, reply
    assert CHARACTER_MENU_PROMPT in reply, reply
    mortal.close()
    return server.login(character.name)


def wait_for_log_line(log_path: Path, line: str) -> None:
    """Waits until game.log holds line; fails the test naming the line when it never arrives."""
    deadline = time.monotonic() + LOG_TIMEOUT
    while True:
        text = log_path.read_text(encoding="latin-1", errors="replace") if log_path.exists() else ""
        if line in text:
            return
        if time.monotonic() >= deadline:
            refreshes = [log_line for log_line in text.splitlines() if "Object version refresh" in log_line]
            raise AssertionError(f"game.log has no {line!r} within {LOG_TIMEOUT}s; refresh lines: {refreshes!r}")
        time.sleep(LOG_POLL_SECONDS)


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR)
def test_stat_shows_no_version_and_no_enchants_on_a_plain_copy(server) -> None:
    """Pins #343's stat lines on an object nobody has versioned or enchanted.

    stat obj shows the prototype's version and the copy's, both `none` while the prototype's
    version is 0, and the recorded enchants, `none` for a plain copy.
    """
    implementor = server.login(IMPLEMENTOR.name)
    created = implementor.command(f"load obj {PLAIN_SWORD_VNUM}")
    assert created.contains(CREATED_MARKER), created.text

    stat_lines = stat_sword(implementor)
    assert UNVERSIONED_LINE in stat_lines, stat_lines
    assert NO_ENCHANTS_LINE in stat_lines, stat_lines


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MORTAL_MYSTIC)
@pytest.mark.parametrize(
    ("alignment", "glow_colour", "alignment_name"),
    [
        (GOOD_ALIGNMENT, "blue", "good"),
        (EVIL_ALIGNMENT, "red", "evil"),
        (NEUTRAL_ALIGNMENT, "yellow", "neutral"),
    ],
)
def test_enchant_weapon_records_the_enchant_with_the_casters_alignment(server, alignment, glow_colour, alignment_name) -> None:
    """Pins #343: a successful enchant weapon is recorded on the item with the caster's alignment.

    The caster reads the glow for its alignment, and stat obj lists the recorded enchant,
    `ob +6` and the alignment, beside the +6 OB affect the spell puts on the sword.
    The one draw that can stop the cast is the concentration roll in do_cast, which
    cast_enchant_weapon repeats; the affect-hours schedule keeps everything else from drawing.
    """
    implementor = server.login(IMPLEMENTOR.name)
    mystic = server.login(MORTAL_MYSTIC.name)
    prepare_caster(implementor, MORTAL_MYSTIC, alignment)
    give_plain_sword(implementor, MORTAL_MYSTIC)

    reply = cast_enchant_weapon(mystic)
    assert GLOW_MARKER_FORMAT.format(glow_colour) in reply, reply

    stat_lines = stat_sword(implementor)
    assert ENCHANT_LINE_FORMAT.format(alignment_name) in stat_lines, stat_lines
    assert ENCHANT_AFFECTIONS_LINE in stat_lines, stat_lines


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MORTAL_MYSTIC)
def test_a_second_enchant_weapon_on_the_same_sword_does_nothing(server) -> None:
    """Pins today's refusal of a second enchant weapon on an enchanted sword: nothing happens.

    The first cast sets the MAGIC flag, and spell_enchant_weapon (mystic.cpp) acts only on a
    weapon without it, so the second cast goes off (`Ok.`) and then prints nothing: no glow,
    and not `There is too much magic in it already.`, which is only for a weapon that has an
    affect without the MAGIC flag. stat obj still lists the one enchant the first cast recorded.
    The one draw that can stop a cast is the concentration roll in do_cast, which
    cast_enchant_weapon repeats; the affect-hours schedule keeps everything else from drawing.
    """
    implementor = server.login(IMPLEMENTOR.name)
    mystic = server.login(MORTAL_MYSTIC.name)
    prepare_caster(implementor, MORTAL_MYSTIC, GOOD_ALIGNMENT)
    give_plain_sword(implementor, MORTAL_MYSTIC)

    first_reply = cast_enchant_weapon(mystic)
    assert GLOW_MARKER_FORMAT.format("blue") in first_reply, first_reply

    second_reply = cast_enchant_weapon(mystic)
    assert CAST_DONE_MARKER in second_reply, second_reply
    assert GLOW_FRAGMENT not in second_reply, second_reply
    assert TOO_MUCH_MAGIC_MARKER not in second_reply, second_reply

    stat_lines = stat_sword(implementor)
    assert ENCHANT_LINE_FORMAT.format("good") in stat_lines, stat_lines


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, OWNER)
def test_a_version_bump_refreshes_the_owners_copy_at_the_next_login_and_keeps_its_enchant(server) -> None:
    """Pins #343: after a version bump, the owner's saved copy is refreshed at its next login and
    keeps its recorded enchant, since the bumped prototype can still take it.

    The owner enchants its sword; the implementor bumps the prototype's version in the object
    editor (field 22, `b`) and implements it. The copy online is not refreshed. The owner quits
    and logs in again: it reads that the sword was updated, stat obj shows the copy stamped with
    the prototype's version, the enchant and its +6 OB still on it, and the server log records
    the refresh with `enchant kept`.
    The one draw that can stop the cast is the concentration roll in do_cast, which
    cast_enchant_weapon repeats; the affect-hours schedule keeps everything else from drawing.
    """
    implementor = server.login(IMPLEMENTOR.name)
    owner = server.login(OWNER.name)
    prepare_caster(implementor, OWNER, GOOD_ALIGNMENT)
    give_plain_sword(implementor, OWNER)
    owner_idnum = player_idnum(implementor, OWNER.name)

    reply = cast_enchant_weapon(owner)
    assert GLOW_MARKER_FORMAT.format("blue") in reply, reply
    enchanted_lines = stat_sword(implementor)
    assert UNVERSIONED_LINE in enchanted_lines, enchanted_lines
    assert ENCHANT_LINE_FORMAT.format("good") in enchanted_lines, enchanted_lines

    shape_sword_prototype(implementor, give_strength_affect=False)
    online_lines = stat_sword(implementor)
    assert BUMPED_BEFORE_LOGIN_LINE in online_lines, online_lines

    owner = quit_and_log_in_again(server, owner, OWNER)
    assert UPDATED_MARKER_FORMAT.format(SWORD_SHORT_DESCRIPTION) in owner.everything, owner.everything[-QUOTED_TEXT_LENGTH:]

    refreshed_lines = stat_sword(implementor)
    assert REFRESHED_LINE in refreshed_lines, refreshed_lines
    assert ENCHANT_LINE_FORMAT.format("good") in refreshed_lines, refreshed_lines
    assert ENCHANT_AFFECTIONS_LINE in refreshed_lines, refreshed_lines
    wait_for_log_line(server.run_dir / "game.log", REFRESH_LOG_FORMAT.format(owner_idnum, "kept"))


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MORTAL_MYSTIC)
def test_a_version_bump_refreshes_a_mortals_rented_copy_when_it_returns_and_keeps_its_enchant(server) -> None:
    """Pins #343 for a mortal: the copy a mortal saves by renting at an inn is refreshed when it
    returns, and keeps its recorded enchant, since the bumped prototype can still take it.

    The mortal enchants its sword; the implementor bumps the prototype's version in the object
    editor and implements it, then teleports the mortal into the test world's inn. The mortal
    rents: the innkeeper stores its belongings and the connection returns to the character menu.
    Logged in again, the mortal is back in the inn and reads that the sword was updated; stat obj
    shows the copy stamped with the prototype's version, the enchant and its +6 OB still on it,
    and the server log records the refresh with `enchant kept`. Rent costs nothing
    (Crash_offer_rent and Crash_rentsave, objsave.cpp), so the mortal needs no gold.
    The one draw that can stop the cast is the concentration roll in do_cast, which
    cast_enchant_weapon repeats. The affect-hours schedule keeps the world from drawing; the
    innkeeper's special draws for the login's look in the inn, which decides nothing here.
    """
    implementor = server.login(IMPLEMENTOR.name)
    mystic = server.login(MORTAL_MYSTIC.name)
    prepare_caster(implementor, MORTAL_MYSTIC, GOOD_ALIGNMENT)
    give_plain_sword(implementor, MORTAL_MYSTIC)
    mystic_idnum = player_idnum(implementor, MORTAL_MYSTIC.name)

    reply = cast_enchant_weapon(mystic)
    assert GLOW_MARKER_FORMAT.format("blue") in reply, reply

    shape_sword_prototype(implementor, give_strength_affect=False)
    online_lines = stat_sword(implementor)
    assert BUMPED_BEFORE_LOGIN_LINE in online_lines, online_lines

    send_to_inn(implementor, mystic, MORTAL_MYSTIC)
    mystic = rent_and_log_in_again(server, mystic, MORTAL_MYSTIC)
    returned_text = mystic.everything
    assert UPDATED_MARKER_FORMAT.format(SWORD_SHORT_DESCRIPTION) in returned_text, returned_text[-QUOTED_TEXT_LENGTH:]
    assert INN_ROOM_NAME in returned_text, returned_text[-QUOTED_TEXT_LENGTH:]

    refreshed_lines = stat_sword(implementor)
    assert REFRESHED_LINE in refreshed_lines, refreshed_lines
    assert ENCHANT_LINE_FORMAT.format("good") in refreshed_lines, refreshed_lines
    assert ENCHANT_AFFECTIONS_LINE in refreshed_lines, refreshed_lines
    wait_for_log_line(server.run_dir / "game.log", REFRESH_LOG_FORMAT.format(mystic_idnum, "kept"))


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MORTAL_MYSTIC)
def test_the_test_innkeeper_quotes_free_rent_for_what_a_mortal_carries(server) -> None:
    """Pins the test world's innkeeper fixture: mob 1105 in the inn has the receptionist special
    (spec_ass.cpp), so a mortal's offer there gets its quote.

    The quote is the day's cost of the level and the carried sword (Crash_offer_rent,
    objsave.cpp); the second line says the mortal's gold lasts a lifetime, because the code zeroes
    the cost before comparing it with the gold. Nothing on this path draws.
    """
    implementor = server.login(IMPLEMENTOR.name)
    mystic = server.login(MORTAL_MYSTIC.name)
    give_plain_sword(implementor, MORTAL_MYSTIC)
    send_to_inn(implementor, mystic, MORTAL_MYSTIC)

    # The quote, with factor RENT_FACTOR (1) and RENT_HALFTIME 300 (objsave.cpp):
    #   totalcost = level * factor + (cost_per_day(sword) >> 1) * factor
    #             = 30 * 1         + ((2000 / 100) / 8 >> 1) * 1
    #               (Testmystic's level; the sword's cost 2000, rent -1 and level 1, obj/16.obj)
    #             = 30 + (2 >> 1) = 31
    #   shown     = totalcost * RENT_HALFTIME * 24 / (RENT_HALFTIME + 24)
    #             = 31 * 300 * 24 / 324 = 688 copper, "6 silver and 88 copper" (money_message)
    mystic.send_line("offer")
    reply = mystic.expect([LIFETIME_OF_RENT_MARKER, NOTHING_TO_STORE_MARKER, CANNOT_SEE_RENTER_MARKER, NOT_HERE_MARKER]) + mystic.drain(SETTLE_SECONDS)
    assert OFFER_QUOTE_MARKER in reply, reply
    assert LIFETIME_OF_RENT_MARKER in reply, reply


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, OWNER)
def test_a_version_bump_to_a_prototype_that_cannot_take_the_enchant_drops_it(server) -> None:
    """Pins #343: a refresh drops a recorded enchant the bumped prototype can no longer take.

    The owner enchants its sword; the implementor gives the prototype a strength affect in the
    object editor (field 19), so object_takes_enchant (utility.cpp) refuses it, then bumps the
    version and implements it. After the owner quits and logs in again, the copy carries the
    prototype's affect instead of the enchant's, stat obj lists no enchant, and the server log
    records the refresh with `enchant dropped`.
    The one draw that can stop the cast is the concentration roll in do_cast, which
    cast_enchant_weapon repeats; the affect-hours schedule keeps everything else from drawing.
    """
    implementor = server.login(IMPLEMENTOR.name)
    owner = server.login(OWNER.name)
    prepare_caster(implementor, OWNER, GOOD_ALIGNMENT)
    give_plain_sword(implementor, OWNER)
    owner_idnum = player_idnum(implementor, OWNER.name)

    reply = cast_enchant_weapon(owner)
    assert GLOW_MARKER_FORMAT.format("blue") in reply, reply

    shape_sword_prototype(implementor, give_strength_affect=True)
    owner = quit_and_log_in_again(server, owner, OWNER)
    assert UPDATED_MARKER_FORMAT.format(SWORD_SHORT_DESCRIPTION) in owner.everything, owner.everything[-QUOTED_TEXT_LENGTH:]

    refreshed_lines = stat_sword(implementor)
    assert REFRESHED_LINE in refreshed_lines, refreshed_lines
    assert NO_ENCHANTS_LINE in refreshed_lines, refreshed_lines
    assert STRENGTH_AFFECTIONS_LINE in refreshed_lines, refreshed_lines
    wait_for_log_line(server.run_dir / "game.log", REFRESH_LOG_FORMAT.format(owner_idnum, "dropped"))
