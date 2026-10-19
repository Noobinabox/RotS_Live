"""Followers left behind: the ACK lines a leader and its follower read when the follower cannot
follow, and the control where a standing follower follows."""

from __future__ import annotations

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.followers]

# The leader: the implementor, male, so the follower's line names him as "him".
LEADER = IMPLEMENTOR
# A level-10 human warrior in the arena centre. Female, so the leader's line ("you lost her!")
# and the follower's ("follow him!") each show whose pronoun act() used.
FOLLOWER = Character(
    name="Testtrailer",
    race="human",
    sex="female",
    level=10,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="warrior",
)
# A level-10 human mage in the arena centre who knows cure self fully: 100 practice sessions put
# its knowledge at the cap of 100 (recalc_skills, spec_pro.cpp). Female for the same reason.
CASTING_FOLLOWER = Character(
    name="Testchanter",
    race="human",
    sex="female",
    level=10,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="mage",
    skills=(("cure_self", 100),),
)

# The room the leader walks to: Arena East, east of the arena centre.
DESTINATION_ROOM_NAME = "Arena East"
# The end of what the follower reads as the leader walks east (do_move, act_move.cpp), sent
# before any of the follower's outcomes.
LEADER_LEAVES_MARKER = "leaves east."
# What the leader reads when a follower is left behind (do_move, act_move.cpp); format with the
# follower's name. Its pronoun is the follower's: the act() actor is the follower.
LEADER_ACK_MARKER_FORMAT = "ACK! {} could not follow, you lost her!"
# What the follower reads when it is left behind (do_move): the pronoun is the leader's.
FOLLOWER_ACK_MARKER = "ACK! You could not follow him!"
# What a follower reads when it follows (do_move); format with the leader's name.
FOLLOWED_MARKER_FORMAT = "You follow {}."
# What command_interpreter (interpre.cpp) tells a character it is asked to run a command for
# while a delay is running.
BUSY_MARKER = "You are busy with something else."
# What a follower without the moves for the step reads (do_move, res_flag 3 from
# check_simple_move).
EXHAUSTED_MARKER = "You are too exhausted to move."
# What the follower reads when it starts following (add_follower, handler.cpp); format with the
# leader's name.
NOW_FOLLOWING_MARKER_FORMAT = "You now follow {}."
# What the caster reads when the casting delay starts (do_cast, spell_pa.cpp).
CONCENTRATE_MARKER = "You start to concentrate."
# What the caster reads when its mana does not cover the cast (can_cast_spell, spell_pa.cpp).
NO_ENERGY_MARKER = "You can't summon enough energy to cast the spell."
# The outcomes of cure self when the delay ends: the concentration roll fails (do_cast), or the
# spell runs on a caster at full hit points or below them (spell_cure_self, mage.cpp).
CAST_OUTCOME_MARKERS = ["You lost your concentration!", "You are already healthy.", "You feel better."]
# Seconds to wait for the cast's outcome: cure self's 36 beats at mage level 10 are
# 36 * 30 / (30 + 10) = 27 passes, under seven seconds (CASTING_TIME, spells.h); 20 leaves room
# for a loaded host.
CAST_TIMEOUT = 20.0
# Seconds to keep reading after an outcome, for text already sent to arrive.
SETTLE_SECONDS = 1.0


def walk_east(leader: GameSession, follower: GameSession) -> tuple:
    """Walks the leader east; returns what the follower and then the leader read.

    Every outcome for either side arrives in the pass that runs the step: the follower reads the
    leader leave first, the leader reads its new room first, and the drain collects the rest.
    """
    leader.send_line("east")
    follower_text = follower.expect([LEADER_LEAVES_MARKER]) + follower.drain(SETTLE_SECONDS)
    leader_text = leader.expect([DESTINATION_ROOM_NAME]) + leader.drain(SETTLE_SECONDS)
    return follower_text, leader_text


def gather_in_arena_centre(leader: GameSession, follower: GameSession) -> None:
    """Brings the leader to the arena centre and has the follower follow it."""
    arrived = leader.command(f"goto {world.ROOM_ARENA_CENTRE}")
    assert arrived.contains("Arena Centre"), arrived.text
    following = follower.command(f"follow {LEADER.name.lower()}")
    assert following.contains(NOW_FOLLOWING_MARKER_FORMAT.format(LEADER.name)), following.text
    leader.drain(SETTLE_SECONDS)


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(LEADER, FOLLOWER)
def test_a_sitting_follower_left_behind_gets_ack_lines_and_a_standing_one_follows(server) -> None:
    """Pins #389: a follower that is not standing is told it was left behind, and so is its leader.

    do_move (act_move.cpp) never asks a follower below POSITION_STANDING to move; since #389 it
    sends both ACK lines instead of leaving them unsaid. The control has the same follower stand
    and walk after the same leader. No draw decides either outcome: the affect-hours schedule
    keeps mobs still, and the leader's move cost draws but cannot stop an implementor.
    """
    leader = server.login(LEADER.name)
    follower = server.login(FOLLOWER.name)
    gather_in_arena_centre(leader, follower)

    assert follower.command("sit").contains("You sit down."), follower.everything
    left_behind, leader_text = walk_east(leader, follower)
    assert FOLLOWER_ACK_MARKER in left_behind, left_behind
    leader_ack = LEADER_ACK_MARKER_FORMAT.format(FOLLOWER.name)
    assert leader_ack in leader_text, leader_text
    assert follower.command("look").room_name() == "Arena Centre"

    # Control: the leader comes back, the follower stands, and the same step east is taken.
    assert leader.command("west").room_name() == "Arena Centre"
    assert follower.command("stand").contains("You stand up."), follower.everything
    followed, leader_text = walk_east(leader, follower)
    assert FOLLOWED_MARKER_FORMAT.format(LEADER.name) in followed, followed
    assert FOLLOWER_ACK_MARKER not in followed, followed
    assert DESTINATION_ROOM_NAME in followed, followed
    assert leader_ack not in leader_text, leader_text


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(LEADER, CASTING_FOLLOWER)
def test_a_follower_in_a_casting_delay_gets_the_ack_line_instead_of_busy(server) -> None:
    """Pins #389: a standing follower still in a delay is told it was left behind, not that it is busy.

    Before #389 do_move asked a delayed follower to move, and command_interpreter
    (interpre.cpp) answered "You are busy with something else."; now do_move sends both ACK
    lines when the follower's delay is running. Cure self's casting delay is 27 passes (about
    seven seconds), so it is still running when the leader's step, sent after the follower's
    "You start to concentrate.", arrives. Nothing on the way draws before the leader moves; the
    concentration roll at the end of the cast decides only which outcome line follows.
    """
    leader = server.login(LEADER.name)
    follower = server.login(CASTING_FOLLOWER.name)
    gather_in_arena_centre(leader, follower)

    follower.send_line("cast 'cure self'")
    started = follower.expect([CONCENTRATE_MARKER, NO_ENERGY_MARKER])
    assert CONCENTRATE_MARKER in started, started
    left_behind, leader_text = walk_east(leader, follower)
    assert BUSY_MARKER not in left_behind, left_behind
    assert FOLLOWER_ACK_MARKER in left_behind, left_behind
    assert LEADER_ACK_MARKER_FORMAT.format(CASTING_FOLLOWER.name) in leader_text, leader_text

    cast_end = follower.expect(CAST_OUTCOME_MARKERS, timeout=CAST_TIMEOUT) + follower.drain(SETTLE_SECONDS)
    assert BUSY_MARKER not in cast_end, cast_end
    assert follower.command("look").room_name() == "Arena Centre"


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(LEADER, FOLLOWER)
def test_a_follower_without_moves_left_behind_gets_ack_lines(server) -> None:
    """Pins the ACK lines for a standing follower whose own step fails, which #389 kept.

    The follower stands and has no delay, so do_move (act_move.cpp) asks it to move; its own
    do_move call then fails in check_simple_move, which returns 3 because no room costs less than
    one move, and sends the ACK lines before "You are too exhausted to move.". #389 left these
    lines as they were and only stopped them naming a leader that is gone. Under affect-hours
    no hour runs, so the follower's moves stay at 0.
    """
    leader = server.login(LEADER.name)
    follower = server.login(FOLLOWER.name)
    gather_in_arena_centre(leader, follower)

    exhausted = leader.command(f"wizset player {FOLLOWER.name.lower()} move 0")
    assert exhausted.contains(f"{FOLLOWER.name}'s move set to 0."), exhausted.text
    left_behind, leader_text = walk_east(leader, follower)
    assert EXHAUSTED_MARKER in left_behind, left_behind
    assert FOLLOWER_ACK_MARKER in left_behind, left_behind
    assert LEADER_ACK_MARKER_FORMAT.format(FOLLOWER.name) in leader_text, leader_text
    assert follower.command("look").room_name() == "Arena Centre"
