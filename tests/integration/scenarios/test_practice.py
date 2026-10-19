"""Practice at a guildmaster: counted and `all` practice, its refusals, and the sessions-spent
column of the guildmaster's list."""

from __future__ import annotations

import re

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.practice]

# A level-30 human mage in the arena centre who has practiced nothing: the traveller teaches
# humans (its will_teach mask, mob/guildmasters.mob) and teaches mage spells.
PUPIL = Character(
    name="Testpupil",
    race="human",
    sex="female",
    level=30,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="mage",
)
# The sessions the listed pupil has already spent on blink.
BLINK_SESSIONS_SPENT = 12
# The same kind of pupil with those blink sessions spent, for the sessions-spent column.
LISTED_PUPIL = Character(
    name="Testlisted",
    race="human",
    sex="female",
    level=30,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="mage",
    skills=(("blink", BLINK_SESSIONS_SPENT),),
)

# The traveller (tests/integration/world/mob/guildmasters.mob): program 7, the TRAVELLER (MAGE)
# row of guildmasters[] (consts.cpp), which teaches blink up to 100 and cure self up to 80.
GUILDMASTER_VNUM = 1503
# What the implementor reads when the guildmaster appears (do_load, act_wiz.cpp).
GUILDMASTER_CREATED_MARKER = "You create a mysterious traveller."
# The practice sessions the implementor gives the pupil before practicing.
STARTING_SESSIONS = 20

# The guildmaster list's first line (guildmasters[], consts.cpp); format with the pupil's name.
LIST_HEADER_FORMAT = "Hello, {}! I can teach you these spells:"
# The guildmaster list's session count (SPECIAL(guild), spec_pro.cpp).
GUILD_SESSIONS_PATTERN = re.compile(r"You have (\d+) practice sessions left\.")
# A guildmaster list row: name, knowledge, sessions spent, what the guildmaster teaches it to.
# Rows end in "\n\r", so each starts after a carriage return.
GUILD_ROW_PATTERN_FORMAT = r"^\r?{}\s+\d+%\s+(\d+)\s+Taught to: "
# The first line of the plain practice list away from a guildmaster (do_practice, spec_pro.cpp).
PLAIN_SESSIONS_MARKER = "practice sessions left"
# A plain practice list row: the name, then how well it is known in parentheses (how_good).
PLAIN_ROW_PATTERN_FORMAT = r"^\r?{}\s+\([A-Za-z ]+\)"

# What the pupil reads on the first session of a practice command (guildmasters[]).
PRACTICED_MARKER = "You practice it for a while."
# What the others in the room read on that first session; format with the pupil's name.
TEACHES_MARKER_FORMAT = "A mysterious traveller teaches {} for a while."
# What the pupil reads after more than one session (SPECIAL(guild)): the count and the skill.
SESSIONS_SPENT_PATTERN = re.compile(r"You spend (\d+) sessions practicing ([a-z ]+)\.")
# What the pupil reads when the guildmaster teaches the skill no better (guildmasters[]).
LIMIT_MARKER = "The guildmaster bows and tells you 'Sorry, I can not teach you better.'"
# What the pupil reads when the skill is already at 100 (guildmasters[]).
PERFECT_MARKER = "You're already perfect in this."
# What the pupil reads after 51 sessions in one skill (practice_refusal, spec_pro.cpp).
TOO_LONG_MARKER = "You studied this for too long. Enough for you."
# What the pupil reads when its sessions run out (practice_refusal).
NO_SESSIONS_MARKER = "You have no practice sessions left now."
# The refusals SPECIAL(guild) gives before any session is spent.
HOW_MANY_MARKER = "Practice how many times?"
WHICH_SKILL_MARKER = "Practice which skill?"
NO_SUCH_SKILL_MARKER = "There is no such skill as you seek."

# Every line a practice command at the traveller can end with: the outcomes of SPECIAL(guild).
PRACTICE_OUTCOMES = [
    PRACTICED_MARKER,
    LIMIT_MARKER,
    PERFECT_MARKER,
    TOO_LONG_MARKER,
    NO_SESSIONS_MARKER,
    HOW_MANY_MARKER,
    WHICH_SKILL_MARKER,
    NO_SUCH_SKILL_MARKER,
    "I know nothing about it.",
    "You are not dedicated enough for this.",
    "You are not experienced enough for this.",
]
# Seconds to keep reading after an outcome, for text already sent to arrive.
SETTLE_SECONDS = 1.0


def give_sessions(implementor: GameSession, name: str, sessions: int) -> None:
    """Sets the character's practice sessions to exactly sessions."""
    reply = implementor.command(f"wizset player {name.lower()} practices {sessions}")
    assert reply.contains(f"{name}'s practices set to {sessions}."), reply.text


def bring_guildmaster(implementor: GameSession) -> None:
    """Moves the implementor to the arena centre and loads the traveller there."""
    implementor.command(f"goto {world.ROOM_ARENA_CENTRE}")
    reply = implementor.command(f"load mob {GUILDMASTER_VNUM}")
    assert reply.contains(GUILDMASTER_CREATED_MARKER), reply.text


def guild_list(pupil: GameSession, name: str) -> str:
    """The traveller's list as the pupil reads it after a bare `practice`."""
    reply = pupil.command("practice")
    assert reply.contains(LIST_HEADER_FORMAT.format(name)), reply.text
    return reply.text


def sessions_left(pupil: GameSession, name: str) -> int:
    """The practice sessions the traveller's list reports for the pupil."""
    text = guild_list(pupil, name)
    match = GUILD_SESSIONS_PATTERN.search(text)
    assert match is not None, f"no session count in the guildmaster's list:\n{text}"
    return int(match.group(1))


def practice(pupil: GameSession, arguments: str) -> str:
    """Sends `practice <arguments>` at the traveller and returns everything the pupil read."""
    pupil.send_line(f"practice {arguments}")
    return pupil.expect(PRACTICE_OUTCOMES) + pupil.drain(SETTLE_SECONDS)


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, PUPIL)
def test_practice_all_stops_at_the_guildmasters_limit_and_spends_nothing_more(server) -> None:
    """`practice <skill> all` spends sessions only up to the guildmaster's limit (#271, #402).

    The traveller teaches cure self up to 80. Practicing it `all` shows the practice lines once,
    the total spent, then the limit line, and the session count drops by exactly that total;
    asking again spends nothing and gives the limit line alone. Practice draws no random number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(PUPIL.name)
    give_sessions(implementor, PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor)
    assert sessions_left(pupil, PUPIL.name) == STARTING_SESSIONS

    first_try = practice(pupil, "cure self all")
    assert first_try.count(PRACTICED_MARKER) == 1, first_try
    spent = SESSIONS_SPENT_PATTERN.search(first_try)
    assert spent is not None, first_try
    assert spent.group(2) == "cure self", first_try
    sessions_spent = int(spent.group(1))
    assert 1 < sessions_spent < STARTING_SESSIONS, first_try
    limit_at = first_try.index(LIMIT_MARKER)
    assert first_try.index(PRACTICED_MARKER) < spent.start() < limit_at, first_try
    teaching_seen = implementor.expect([TEACHES_MARKER_FORMAT.format(PUPIL.name)])
    teaching_seen += implementor.drain(SETTLE_SECONDS)
    assert teaching_seen.count(TEACHES_MARKER_FORMAT.format(PUPIL.name)) == 1, teaching_seen
    assert sessions_left(pupil, PUPIL.name) == STARTING_SESSIONS - sessions_spent

    second_try = practice(pupil, "cure self all")
    assert LIMIT_MARKER in second_try, second_try
    assert PRACTICED_MARKER not in second_try, second_try
    assert SESSIONS_SPENT_PATTERN.search(second_try) is None, second_try
    assert sessions_left(pupil, PUPIL.name) == STARTING_SESSIONS - sessions_spent


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, PUPIL)
@pytest.mark.parametrize("arguments", ["blink 3", "3 blink"], ids=["skill-first", "count-first"])
def test_a_counted_practice_spends_exactly_that_many_sessions_in_either_word_order(
    server, arguments: str
) -> None:
    """`practice blink 3` and `practice 3 blink` each spend exactly three sessions (#271, #402).

    Each order starts from a pupil who has never practiced blink: three sessions leave it below
    the traveller's limit of 100, which this pupil passes on the fifth. Practice draws no random
    number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(PUPIL.name)
    give_sessions(implementor, PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor)

    reply = practice(pupil, arguments)
    assert reply.count(PRACTICED_MARKER) == 1, reply
    spent = SESSIONS_SPENT_PATTERN.search(reply)
    assert spent is not None, reply
    assert (int(spent.group(1)), spent.group(2)) == (3, "blink"), reply
    assert LIMIT_MARKER not in reply, reply
    assert sessions_left(pupil, PUPIL.name) == STARTING_SESSIONS - 3


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, PUPIL)
def test_running_out_of_sessions_part_way_stops_with_the_no_sessions_line(server) -> None:
    """With two sessions, `practice blink 5` spends both, then says none are left (#271, #402).

    Practice draws no random number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(PUPIL.name)
    give_sessions(implementor, PUPIL.name, 2)
    bring_guildmaster(implementor)

    reply = practice(pupil, "blink 5")
    assert reply.count(PRACTICED_MARKER) == 1, reply
    spent = SESSIONS_SPENT_PATTERN.search(reply)
    assert spent is not None, reply
    assert (int(spent.group(1)), spent.group(2)) == (2, "blink"), reply
    assert spent.start() < reply.index(NO_SESSIONS_MARKER), reply
    assert sessions_left(pupil, PUPIL.name) == 0


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, PUPIL)
@pytest.mark.parametrize(
    ("arguments", "refusal"),
    [
        ("blink 0", HOW_MANY_MARKER),
        ("5", WHICH_SKILL_MARKER),
        ("zzz 3", NO_SUCH_SKILL_MARKER),
    ],
    ids=["zero-count", "no-skill", "unknown-skill"],
)
def test_a_practice_the_guildmaster_cannot_carry_out_is_refused_and_spends_nothing(
    server, arguments: str, refusal: str
) -> None:
    """A zero count, a count alone and an unknown skill each get their refusal (#271, #402).

    Nothing is practiced and no session is spent.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(PUPIL.name)
    give_sessions(implementor, PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor)

    reply = practice(pupil, arguments)
    assert refusal in reply, reply
    assert PRACTICED_MARKER not in reply, reply
    assert sessions_left(pupil, PUPIL.name) == STARTING_SESSIONS


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, LISTED_PUPIL)
def test_the_guildmaster_list_shows_sessions_spent_and_the_plain_list_does_not(server) -> None:
    """The guildmaster's list has a sessions-spent column; plain `practice` has none (#310, #321).

    Away from a guildmaster, `practice` lists blink with how well it is known and no count; at
    the traveller, the blink row shows the 12 sessions the pupil has spent on it.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(LISTED_PUPIL.name)

    plain = pupil.command("practice")
    assert plain.contains(PLAIN_SESSIONS_MARKER), plain.text
    assert not plain.contains(LIST_HEADER_FORMAT.format(LISTED_PUPIL.name)), plain.text
    plain_row_pattern = PLAIN_ROW_PATTERN_FORMAT.format("blink") + r".*$"
    plain_row = re.search(plain_row_pattern, plain.text, re.MULTILINE)
    assert plain_row is not None, plain.text
    assert "%" not in plain_row.group(0), plain.text
    assert f" {BLINK_SESSIONS_SPENT} " not in plain_row.group(0), plain.text

    bring_guildmaster(implementor)
    listed = guild_list(pupil, LISTED_PUPIL.name)
    guild_row = re.search(GUILD_ROW_PATTERN_FORMAT.format("blink"), listed, re.MULTILINE)
    assert guild_row is not None, listed
    assert int(guild_row.group(1)) == BLINK_SESSIONS_SPENT, listed
