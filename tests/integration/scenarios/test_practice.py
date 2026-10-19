"""Practice at a guildmaster: counted and `all` practice, its refusals, the sessions-spent column,
two guildmasters with different limits, and the sessions a skill takes by ranger coefficient."""

from __future__ import annotations

import re
from dataclasses import dataclass

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character, ProfessionPoints
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.practice]


@dataclass(frozen=True)
class Guildmaster:
    """A guildmaster copy in mob/guildmasters.mob and the lines a pupil reads from it."""

    # The copy's vnum: one spec_ass.cpp binds the guild special to.
    vnum: int
    # The keyword `purge` names it by.
    keyword: str
    # Its short description, as `load mob` and act() print it mid-line.
    short_description: str
    # Its list's first line (guildmasters[], consts.cpp); format with the pupil's name.
    list_header_format: str

    @property
    def line_start(self) -> str:
        """The short description as act() starts a line with it, capitalised."""
        return self.short_description[0].upper() + self.short_description[1:]


# Program 7, the TRAVELLER (MAGE) row of guildmasters[] (consts.cpp): teaches blink up to 100 and
# cure self up to 80, fireball and cone of cold, but not magic missile. It teaches the races in
# its will_teach mask, 63: god, human, dwarf, wood elf, hobbit and high elf, not the beorning.
TRAVELLER = Guildmaster(
    1503, "traveller", "a mysterious traveller", "Hello, {}! I can teach you these spells:"
)
# Scorther, program 16, the LAKETOWN MAGE row: teaches blink and cure self up to 60, magic
# missile and identify, but not fireball. Same will_teach mask.
SCORTHER = Guildmaster(
    10003, "scorther", "Scorther", "Hello, {}! I can only teach you these spells:"
)
# Aroden, program 28, the LAKETOWN RANGER (wanderer) row: teaches awareness, travelling and
# stalking up to 100. Same will_teach mask.
ARODEN = Guildmaster(10054, "aroden", "Aroden", "Hello, {}! I can only teach you these skills:")

# A level-30 human mage in the arena centre who has practiced nothing.
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
# The same kind of pupil with five blink sessions spent, which put blink's knowledge at 101
# (recalc_skills, spec_pro.cpp, for a mage-coefficient 1000 pupil: t = 1000 - 5 * 200 = 0, and
# (10000 - 0) / 99 = 101).
PERFECT_PUPIL = Character(
    name="Testperfect",
    race="human",
    sex="female",
    level=30,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="mage",
    skills=(("blink", 5),),
)
# A level-8 human mage: mage level 8, below fireball's level of 21 (skills[], consts.cpp).
NOVICE_PUPIL = Character(
    name="Testnovice",
    race="human",
    sex="female",
    level=8,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="mage",
)
# A level-30 beorning mage, a race outside the traveller's will_teach mask. Three sessions of
# human language master it (recalc_skills: a general skill at coefficient 1000 and difficulty 5
# passes 100 on the third), so the traveller's speech, in human language, reaches the beorning
# unscrambled.
BEORNING_PUPIL = Character(
    name="Testbear",
    race="beorning",
    sex="male",
    level=30,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="mage",
    skills=(("human_language", 3),),
)
# Level-30 humans whose ranger coefficients differ: the ranger class has 100 ranger points
# (coefficient square_root[100] = 1000), the wizard class 9 (300), and this custom split 7 (264).
RANGER_PUPIL = Character(
    name="Testranger",
    race="human",
    sex="male",
    level=30,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="ranger",
)
WIZARD_PUPIL = Character(
    name="Testwizard",
    race="human",
    sex="male",
    level=30,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="wizard",
)
LOW_RANGER_PUPIL = Character(
    name="Testlowran",
    race="human",
    sex="male",
    level=30,
    load_room=world.ROOM_ARENA_CENTRE,
    points=ProfessionPoints(mage=100, mystic=43, ranger=7, warrior=0),
)
# The sessions that end practice in one skill: practice_refusal refuses once more than 50 are spent.
SESSION_CAP = 51
# The low-ranger build with the cap already spent on travelling, which leaves it at 97%.
CAPPED_PUPIL = Character(
    name="Testcapped",
    race="human",
    sex="male",
    level=30,
    load_room=world.ROOM_ARENA_CENTRE,
    points=ProfessionPoints(mage=100, mystic=43, ranger=7, warrior=0),
    skills=(("travelling", SESSION_CAP),),
)

# The practice sessions the implementor gives a pupil before practicing.
STARTING_SESSIONS = 20
# Enough sessions that a practice command never runs out of them.
PLENTY_OF_SESSIONS = 200
# The specialization stalking needs: PLRSPEC_STLH (structs.h), its skills[] skill_spec.
STEALTH_SPECIALIZATION = 6

# The guildmaster list's session count (SPECIAL(guild), spec_pro.cpp).
GUILD_SESSIONS_PATTERN = re.compile(r"You have (\d+) practice sessions left\.")
# A guildmaster list row: name, knowledge, sessions spent, and what the guildmaster teaches it
# to, as how_good's word. Rows end in "\n\r", so each starts after a carriage return.
GUILD_ROW_PATTERN_FORMAT = r"^\r?{}\s+(\d+)%\s+(\d+)\s+Taught to:\s+\(([A-Za-z ]+)\)"
# The first line of the plain practice list away from a guildmaster (do_practice, spec_pro.cpp).
PLAIN_SESSIONS_MARKER = "practice sessions left"
# A plain practice list row: the name, then how well it is known in parentheses (how_good).
PLAIN_ROW_PATTERN_FORMAT = r"^\r?{}\s+\(([A-Za-z ]+)\)"
# The professions' coefficients and levels in `stat player` (do_stat_character, act_wiz.cpp).
COEFFICIENTS_PATTERN = re.compile(r"Class coofs : Mag:(\d+) Cle:(\d+) Ran:(\d+) War:(\d+)")
CLASS_LEVELS_PATTERN = re.compile(r"Class levels: Mag:(\d+) Cle:(\d+) Ran:(\d+) War:(\d+)")
# The practice sessions in `stat player`.
STAT_SESSIONS_PATTERN = re.compile(r"\(pracs left:\[(-?\d+)\]\)")

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
UNKNOWN_TO_GUILDMASTER_MARKER = "tells you 'I know nothing about it.'"
NOT_DEDICATED_MARKER = "You are not dedicated enough for this."
NOT_EXPERIENCED_MARKER = "You are not experienced enough for this."
# What the guildmaster says to a race outside its will_teach mask (SPECIAL(guild), do_say).
GO_AWAY_SPEECH = "says 'Go away, I won't teach you anything!'"

# Every line a practice command at a guildmaster can end with: the outcomes of SPECIAL(guild).
PRACTICE_OUTCOMES = [
    PRACTICED_MARKER,
    LIMIT_MARKER,
    PERFECT_MARKER,
    TOO_LONG_MARKER,
    NO_SESSIONS_MARKER,
    HOW_MANY_MARKER,
    WHICH_SKILL_MARKER,
    NO_SUCH_SKILL_MARKER,
    UNKNOWN_TO_GUILDMASTER_MARKER,
    NOT_DEDICATED_MARKER,
    NOT_EXPERIENCED_MARKER,
    GO_AWAY_SPEECH,
]
# Seconds to keep reading after an outcome, for text already sent to arrive.
SETTLE_SECONDS = 1.0


def give_sessions(implementor: GameSession, name: str, sessions: int) -> None:
    """Sets the character's practice sessions to exactly sessions."""
    reply = implementor.command(f"wizset player {name.lower()} practices {sessions}")
    assert reply.contains(f"{name}'s practices set to {sessions}."), reply.text


def bring_guildmaster(implementor: GameSession, guildmaster: Guildmaster = TRAVELLER) -> None:
    """Moves the implementor to the arena centre and loads the guildmaster there."""
    implementor.command(f"goto {world.ROOM_ARENA_CENTRE}")
    reply = implementor.command(f"load mob {guildmaster.vnum}")
    assert reply.contains(f"You create {guildmaster.short_description}."), reply.text


def dismiss_guildmaster(implementor: GameSession, guildmaster: Guildmaster) -> None:
    """Purges the guildmaster from the implementor's room, so the next one loaded answers alone."""
    reply = implementor.command(f"purge {guildmaster.keyword}")
    assert reply.contains("Ok."), reply.text


def guild_list(pupil: GameSession, name: str, guildmaster: Guildmaster = TRAVELLER) -> str:
    """The guildmaster's list as the pupil reads it after a bare `practice`."""
    reply = pupil.command("practice")
    assert reply.contains(guildmaster.list_header_format.format(name)), reply.text
    return reply.text


def guild_row(listed: str, skill: str) -> tuple[int, int, str]:
    """A skill's knowledge, sessions spent and taught-to word in a guildmaster's list."""
    row = re.search(GUILD_ROW_PATTERN_FORMAT.format(re.escape(skill)), listed, re.MULTILINE)
    assert row is not None, f"no {skill} row in the guildmaster's list:\n{listed}"
    return int(row.group(1)), int(row.group(2)), row.group(3)


def listed_sessions(listed: str) -> int:
    """The practice sessions a guildmaster's list reports."""
    match = GUILD_SESSIONS_PATTERN.search(listed)
    assert match is not None, f"no session count in the guildmaster's list:\n{listed}"
    return int(match.group(1))


def sessions_left(pupil: GameSession, name: str, guildmaster: Guildmaster = TRAVELLER) -> int:
    """The practice sessions the guildmaster's list reports for the pupil."""
    return listed_sessions(guild_list(pupil, name, guildmaster))


def practice(pupil: GameSession, arguments: str) -> str:
    """Sends `practice <arguments>` at a guildmaster and returns everything the pupil read."""
    pupil.send_line(f"practice {arguments}")
    return pupil.expect(PRACTICE_OUTCOMES) + pupil.drain(SETTLE_SECONDS)


def stat_player(implementor: GameSession, name: str) -> str:
    """The implementor's `stat player` of the character, read to its last line."""
    implementor.send_line(f"stat player {name.lower()}")
    return implementor.expect(["VUL:"]) + implementor.drain(SETTLE_SECONDS)


def ranger_coefficient_and_level(implementor: GameSession, name: str) -> tuple[int, int]:
    """The character's ranger coefficient and ranger level, from `stat player`."""
    text = stat_player(implementor, name)
    coefficients = COEFFICIENTS_PATTERN.search(text)
    levels = CLASS_LEVELS_PATTERN.search(text)
    assert coefficients is not None and levels is not None, text
    return int(coefficients.group(3)), int(levels.group(3))


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
    assert not plain.contains(TRAVELLER.list_header_format.format(LISTED_PUPIL.name)), plain.text
    plain_row_pattern = PLAIN_ROW_PATTERN_FORMAT.format("blink") + r".*$"
    plain_row = re.search(plain_row_pattern, plain.text, re.MULTILINE)
    assert plain_row is not None, plain.text
    assert "%" not in plain_row.group(0), plain.text
    assert f" {BLINK_SESSIONS_SPENT} " not in plain_row.group(0), plain.text

    bring_guildmaster(implementor)
    listed = guild_list(pupil, LISTED_PUPIL.name)
    assert guild_row(listed, "blink")[1] == BLINK_SESSIONS_SPENT, listed


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, PUPIL)
def test_a_skill_stopped_at_one_guildmasters_limit_is_finished_at_a_higher_one(server) -> None:
    """Blink practised to Scorther's limit of 60 goes on at the traveller, who teaches it to 100 (#402).

    With a mage coefficient of 1000, blink's knowledge after N sessions is 36, 64, 84, 96, 101
    (recalc_skills, spec_pro.cpp). At Scorther the second session passes 60, is cut back to it,
    and the limit line ends the command; the traveller's list then shows the same 60% with its
    own higher limit, and three more sessions reach 100 (the fifth passes it and is cut back, so
    the limit line ends this command too). Only one guildmaster is in the room at a time.
    Practice draws no random number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(PUPIL.name)
    give_sessions(implementor, PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor, SCORTHER)

    at_scorther = practice(pupil, "blink all")
    spent = SESSIONS_SPENT_PATTERN.search(at_scorther)
    assert spent is not None, at_scorther
    assert (int(spent.group(1)), spent.group(2)) == (2, "blink"), at_scorther
    assert LIMIT_MARKER in at_scorther, at_scorther
    assert spent.start() < at_scorther.index(LIMIT_MARKER), at_scorther
    scorther_list = guild_list(pupil, PUPIL.name, SCORTHER)
    assert guild_row(scorther_list, "blink") == (60, 2, "fair"), scorther_list

    dismiss_guildmaster(implementor, SCORTHER)
    bring_guildmaster(implementor, TRAVELLER)
    traveller_list = guild_list(pupil, PUPIL.name, TRAVELLER)
    assert guild_row(traveller_list, "blink") == (60, 2, "Mastered"), traveller_list

    at_traveller = practice(pupil, "blink all")
    spent = SESSIONS_SPENT_PATTERN.search(at_traveller)
    assert spent is not None, at_traveller
    assert (int(spent.group(1)), spent.group(2)) == (3, "blink"), at_traveller
    assert LIMIT_MARKER in at_traveller, at_traveller
    assert spent.start() < at_traveller.index(LIMIT_MARKER), at_traveller
    finished_list = guild_list(pupil, PUPIL.name, TRAVELLER)
    assert guild_row(finished_list, "blink") == (100, 5, "Mastered"), finished_list
    assert listed_sessions(finished_list) == STARTING_SESSIONS - 5


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, PUPIL)
def test_a_lower_limit_guildmaster_refuses_a_skill_already_past_it_and_spends_nothing(
    server,
) -> None:
    """Cure self practised to the traveller's 80 is refused at once by Scorther, who teaches 60 (#402).

    Three sessions take cure self to 84, cut back to the traveller's 80 (recalc_skills gives the
    same 36, 64, 84 as blink at a mage coefficient of 1000). Scorther's limit of 60 is below it,
    so his only answer is the limit line and the session count stays put. Practice draws no
    random number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(PUPIL.name)
    give_sessions(implementor, PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor, TRAVELLER)
    at_traveller = practice(pupil, "cure self all")
    spent = SESSIONS_SPENT_PATTERN.search(at_traveller)
    assert spent is not None, at_traveller
    assert (int(spent.group(1)), spent.group(2)) == (3, "cure self"), at_traveller
    assert guild_row(guild_list(pupil, PUPIL.name, TRAVELLER), "cure self") == (80, 3, "good")

    dismiss_guildmaster(implementor, TRAVELLER)
    bring_guildmaster(implementor, SCORTHER)
    at_scorther = practice(pupil, "cure self")
    assert LIMIT_MARKER in at_scorther, at_scorther
    assert PRACTICED_MARKER not in at_scorther, at_scorther
    scorther_list = guild_list(pupil, PUPIL.name, SCORTHER)
    assert guild_row(scorther_list, "cure self") == (80, 3, "fair"), scorther_list
    assert listed_sessions(scorther_list) == STARTING_SESSIONS - 3


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, PUPIL)
@pytest.mark.parametrize(
    ("refusing", "teaching", "skill"),
    [(TRAVELLER, SCORTHER, "magic missile"), (SCORTHER, TRAVELLER, "fireball")],
    ids=["magic-missile-only-at-scorther", "fireball-only-at-the-traveller"],
)
def test_a_spell_one_guildmaster_does_not_teach_is_refused_there_and_taught_by_the_other(
    server, refusing: Guildmaster, teaching: Guildmaster, skill: str
) -> None:
    """A guildmaster whose row has 0 for a spell refuses it; one with a limit teaches it (long-standing).

    The refusal spends nothing; the other guildmaster's one session spends one. The level-30
    pupil's mage level, 30, covers fireball's level of 21. Practice draws no random number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(PUPIL.name)
    give_sessions(implementor, PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor, refusing)

    refused = practice(pupil, skill)
    assert f"{refusing.line_start} {UNKNOWN_TO_GUILDMASTER_MARKER}" in refused, refused
    assert PRACTICED_MARKER not in refused, refused
    assert sessions_left(pupil, PUPIL.name, refusing) == STARTING_SESSIONS

    dismiss_guildmaster(implementor, refusing)
    bring_guildmaster(implementor, teaching)
    taught = practice(pupil, skill)
    assert PRACTICED_MARKER in taught, taught
    assert UNKNOWN_TO_GUILDMASTER_MARKER not in taught, taught
    teaching_list = guild_list(pupil, PUPIL.name, teaching)
    assert guild_row(teaching_list, skill)[1] == 1, teaching_list
    assert listed_sessions(teaching_list) == STARTING_SESSIONS - 1


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, BEORNING_PUPIL)
def test_a_race_outside_the_guildmasters_mask_is_sent_away(server) -> None:
    """The traveller tells a beorning to go away and teaches it nothing (long-standing).

    WILL_TEACH (utils.h) tests the pupil's race bit, 1 << 6 for a beorning, against the mask 63.
    The refusal is the guildmaster's speech (do_say), which the pupil reads as
    `A mysterious traveller says '...'`; the session count in `stat player` stays put.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(BEORNING_PUPIL.name)
    plain = pupil.command("practice")
    language_pattern = PLAIN_ROW_PATTERN_FORMAT.format("human language")
    language_row = re.search(language_pattern, plain.text, re.MULTILINE)
    assert language_row is not None and language_row.group(1) == "Mastered", plain.text
    give_sessions(implementor, BEORNING_PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor)

    reply = practice(pupil, "blink")
    assert f"{TRAVELLER.line_start} {GO_AWAY_SPEECH}" in reply, reply
    assert PRACTICED_MARKER not in reply, reply
    stat_sessions = STAT_SESSIONS_PATTERN.search(stat_player(implementor, BEORNING_PUPIL.name))
    assert stat_sessions is not None and int(stat_sessions.group(1)) == STARTING_SESSIONS


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, NOVICE_PUPIL)
def test_a_spell_above_the_pupils_mage_level_is_not_taught(server) -> None:
    """A level-8 mage asking the traveller for fireball (level 21) is not experienced enough (long-standing).

    The check compares skills[].level with the pupil's mage level, read here from `stat player`.
    Nothing is practiced and no session is spent.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(NOVICE_PUPIL.name)
    levels = CLASS_LEVELS_PATTERN.search(stat_player(implementor, NOVICE_PUPIL.name))
    assert levels is not None and int(levels.group(1)) == 8
    give_sessions(implementor, NOVICE_PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor)

    reply = practice(pupil, "fireball")
    assert NOT_EXPERIENCED_MARKER in reply, reply
    assert PRACTICED_MARKER not in reply, reply
    assert sessions_left(pupil, NOVICE_PUPIL.name) == STARTING_SESSIONS


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, RANGER_PUPIL)
def test_a_specialization_skill_is_taught_only_to_a_pupil_with_that_specialization(
    server,
) -> None:
    """Aroden refuses stalking without the stealth specialization, then teaches it (long-standing).

    Stalking's learn_type has LEARN_SPEC (skills[], consts.cpp), so SPECIAL(guild) compares the
    pupil's specialization with its skill_spec, stealth. The refusal spends nothing; after
    `wizset ... specialization 6` the same request spends one session. Practice draws no random
    number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(RANGER_PUPIL.name)
    give_sessions(implementor, RANGER_PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor, ARODEN)

    refused = practice(pupil, "stalking")
    assert NOT_DEDICATED_MARKER in refused, refused
    assert PRACTICED_MARKER not in refused, refused
    assert sessions_left(pupil, RANGER_PUPIL.name, ARODEN) == STARTING_SESSIONS

    name = RANGER_PUPIL.name
    reply = implementor.command(f"wizset player {name.lower()} specialization {STEALTH_SPECIALIZATION}")
    assert reply.contains(f"{name}'s specialization set to {STEALTH_SPECIALIZATION}."), reply.text
    taught = practice(pupil, "stalking")
    assert PRACTICED_MARKER in taught, taught
    assert NOT_DEDICATED_MARKER not in taught, taught
    assert sessions_left(pupil, RANGER_PUPIL.name, ARODEN) == STARTING_SESSIONS - 1


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, CAPPED_PUPIL)
def test_a_skill_with_51_sessions_spent_is_refused_at_once(server) -> None:
    """Aroden refuses travelling to a pupil who has spent 51 sessions on it, at 97% (#402).

    practice_refusal's check is `skills > 50`; the knowledge is below Aroden's limit of 100, so
    the session cap is the only refusal that applies. Nothing is practiced and no session is
    spent.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(CAPPED_PUPIL.name)
    give_sessions(implementor, CAPPED_PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor, ARODEN)
    listed = guild_list(pupil, CAPPED_PUPIL.name, ARODEN)
    assert guild_row(listed, "travelling") == (97, SESSION_CAP, "Mastered"), listed

    reply = practice(pupil, "travelling")
    assert TOO_LONG_MARKER in reply, reply
    assert PRACTICED_MARKER not in reply, reply
    assert sessions_left(pupil, CAPPED_PUPIL.name, ARODEN) == STARTING_SESSIONS


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, PERFECT_PUPIL)
def test_a_skill_already_at_100_is_refused_as_perfect(server) -> None:
    """The traveller refuses blink to a pupil whose five sessions put it at 101% (#402).

    practice_refusal checks `knowledge >= 100` first. Nothing is practiced and no session is
    spent.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(PERFECT_PUPIL.name)
    give_sessions(implementor, PERFECT_PUPIL.name, STARTING_SESSIONS)
    bring_guildmaster(implementor)

    reply = practice(pupil, "blink")
    assert PERFECT_MARKER in reply, reply
    assert PRACTICED_MARKER not in reply, reply
    assert sessions_left(pupil, PERFECT_PUPIL.name) == STARTING_SESSIONS


# The knowledge a session brings (recalc_skills, spec_pro.cpp) rests on the sessions spent N, the
# skill's level L and difficulty D (skills[], consts.cpp), and the pupil's ranger coefficient C
# (`stat player`'s "Class coofs" Ran), never its overall level. For a ranger skill with L < 20:
#   tmps      = C - L * (1000 - C) / 30
#   tmps      = MIN(1000, tmps * (80 + L) / 100 + 200 - L * 10)
#   t         = 1000 - (N * 20 * tmps * 10) / (D * 100)
#   knowledge = (10000 - t * t / 100) / 99, or 100 when t < 0
# The command stops at the first check practice_refusal fails: knowledge >= 100 gives the
# perfect line, more than 50 sessions the too-long line.
#
# Awareness, L = 1, D = 20:
#   Testranger, C = 1000: tmps = 1000 - 1 * 0 / 30 = 1000
#                         MIN(1000, 1000 * 81 / 100 + 200 - 10) = 810 + 190 = 1000
#     N = 8: t = 1000 - 8 * 20 * 1000 * 10 / 2000 = 1000 - 800 = 200; (10000 - 400) / 99 = 96
#     N = 9: t = 1000 - 900 = 100; (10000 - 100) / 99 = 100, so 9 sessions, then the perfect line.
#   Testwizard, C = 300: tmps = 300 - 1 * 700 / 30 = 300 - 23 = 277
#                        MIN(1000, 277 * 81 / 100 + 200 - 10) = 224 + 190 = 414
#     N = 21: t = 1000 - 21 * 20 * 414 * 10 / 2000 = 1000 - 869 = 131; (10000 - 171) / 99 = 99
#     N = 22: t = 1000 - 22 * 20 * 414 * 10 / 2000 = 1000 - 910 = 90; (10000 - 81) / 99 = 100,
#     so 22 sessions, then the perfect line.
# Travelling, L = 6, D = 30:
#   Testranger, C = 1000: tmps = 1000 - 6 * 0 / 30 = 1000
#                         MIN(1000, 1000 * 86 / 100 + 200 - 60) = 860 + 140 = 1000
#     N = 13: t = 1000 - 13 * 20 * 1000 * 10 / 3000 = 1000 - 866 = 134; (10000 - 179) / 99 = 99
#     N = 14: t = 1000 - 933 = 67; (10000 - 44) / 99 = 100, so 14 sessions, then the perfect line.
#   Testlowran, C = 264: tmps = 264 - 6 * 736 / 30 = 264 - 147 = 117
#                        MIN(1000, 117 * 86 / 100 + 200 - 60) = 100 + 140 = 240
#     N = 51: t = 1000 - 51 * 20 * 240 * 10 / 3000 = 1000 - 816 = 184; (10000 - 338) / 99 = 97,
#     so the cap of 51 sessions ends it at 97% with the too-long line.
# Awareness cannot show the cap for a human: a ranger level of 1 needs a coefficient of at least
# 34 (advance_mini_level, limits.cpp), which square_root[] makes 100 or more, and C = 100 masters
# awareness in 37 sessions (tmps = 246, t = 1000 - 37 * 20 * 246 * 10 / 2000 = 90).


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, RANGER_PUPIL, WIZARD_PUPIL)
@pytest.mark.parametrize(
    ("pupil_character", "coefficient", "sessions"),
    [(RANGER_PUPIL, 1000, 9), (WIZARD_PUPIL, 300, 22)],
    ids=["ranger", "wizard"],
)
def test_awareness_takes_fewer_sessions_to_master_the_higher_the_ranger_coefficient(
    server, pupil_character: Character, coefficient: int, sessions: int
) -> None:
    """Two level-30 humans master awareness at Aroden in 9 and 22 sessions, by coefficient (#402).

    The counts are worked out in the comment above. Practice draws no random number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(pupil_character.name)
    assert ranger_coefficient_and_level(implementor, pupil_character.name)[0] == coefficient
    give_sessions(implementor, pupil_character.name, PLENTY_OF_SESSIONS)
    bring_guildmaster(implementor, ARODEN)

    reply = practice(pupil, "awareness all")
    spent = SESSIONS_SPENT_PATTERN.search(reply)
    assert spent is not None, reply
    assert (int(spent.group(1)), spent.group(2)) == (sessions, "awareness"), reply
    assert PERFECT_MARKER in reply, reply
    assert spent.start() < reply.index(PERFECT_MARKER), reply
    listed = guild_list(pupil, pupil_character.name, ARODEN)
    assert guild_row(listed, "awareness") == (100, sessions, "Mastered"), listed


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, RANGER_PUPIL, LOW_RANGER_PUPIL)
@pytest.mark.parametrize(
    ("pupil_character", "coefficient", "sessions", "final_line", "knowledge"),
    [
        (RANGER_PUPIL, 1000, 14, PERFECT_MARKER, 100),
        (LOW_RANGER_PUPIL, 264, SESSION_CAP, TOO_LONG_MARKER, 97),
    ],
    ids=["ranger-masters-it", "low-ranger-reaches-the-cap"],
)
def test_a_low_ranger_coefficient_reaches_the_session_cap_before_mastering_travelling(
    server, pupil_character: Character, coefficient: int, sessions: int, final_line: str, knowledge: int
) -> None:
    """A ranger masters travelling in 14 sessions; a 7-point ranger stops at the cap at 97% (#402).

    Travelling (level 6) needs a ranger level of 6, which both pupils have. The counts and
    knowledge are worked out in the comment above. Practice draws no random number.
    """
    implementor = server.login(IMPLEMENTOR.name)
    pupil = server.login(pupil_character.name)
    pupil_coefficient, ranger_level = ranger_coefficient_and_level(implementor, pupil_character.name)
    assert pupil_coefficient == coefficient
    assert ranger_level >= 6
    give_sessions(implementor, pupil_character.name, PLENTY_OF_SESSIONS)
    bring_guildmaster(implementor, ARODEN)

    reply = practice(pupil, "travelling all")
    spent = SESSIONS_SPENT_PATTERN.search(reply)
    assert spent is not None, reply
    assert (int(spent.group(1)), spent.group(2)) == (sessions, "travelling"), reply
    assert final_line in reply, reply
    assert spent.start() < reply.index(final_line), reply
    listed = guild_list(pupil, pupil_character.name, ARODEN)
    assert guild_row(listed, "travelling") == (knowledge, sessions, "Mastered"), listed
    assert listed_sessions(listed) == PLENTY_OF_SESSIONS - sessions
