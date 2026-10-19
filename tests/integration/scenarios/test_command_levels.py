"""Immortal command levels: teleport needs level 93, and shutdown is refused to a level-95
immortal while the implementor still reaches it."""

from __future__ import annotations

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character

pytestmark = [pytest.mark.scenario, pytest.mark.wizlevels]

# A level-93 immortal (LEVEL_GOD, structs.h): the lowest level teleport's row in the command
# table allows (COMMANDO 122, interpre.cpp). Immortals log in to Immortal Start.
GOD = Character(
    name="Testgod",
    race="human",
    sex="male",
    level=93,
    load_room=world.ROOM_IMMORTAL_START,
    class_name="warrior",
)
# A level-92 immortal: one level below teleport's.
LESSER_GOD = Character(
    name="Testdemigod",
    race="human",
    sex="male",
    level=92,
    load_room=world.ROOM_IMMORTAL_START,
    class_name="warrior",
)
# A level-95 immortal (LEVEL_AREAGOD): one level below shutdown's row (COMMANDO 61,
# LEVEL_GRGOD - 1 = 96).
AREA_GOD = Character(
    name="Testareagod",
    race="human",
    sex="male",
    level=95,
    load_room=world.ROOM_IMMORTAL_START,
    class_name="warrior",
)
# A level-12 human warrior in the arena centre: the mortal the immortals try to teleport.
MORTAL = Character(
    name="Testwalker",
    race="human",
    sex="female",
    level=12,
    load_room=world.ROOM_ARENA_CENTRE,
    class_name="warrior",
)

# The room the mortal is teleported to: Arena East (1132).
TELEPORT_DESTINATION = 1132
# What command_interpreter (interpre.cpp) answers when the speaker's level is below the
# command's row, and no social has the word.
UNRECOGNIZED_MARKER = "Unrecognized command."
# What the mortal reads when an immortal teleports it (do_teleport, act_wiz.cpp); format with
# the immortal's name.
TELEPORTED_MARKER_FORMAT = "{} has teleported you!"
# What do_shutdown (act_wiz.cpp) answers an argument that is none of its options: it shuts
# nothing down.
UNKNOWN_OPTION_MARKER = "Unknown shutdown option."
# An argument do_shutdown does not know. Sent instead of a bare `shutdown`, which shuts the
# server down if the level gate ever lets it through.
HARMLESS_SHUTDOWN_ARGUMENT = "foo"
# Seconds to keep reading after a reply, for text that might still arrive.
SETTLE_SECONDS = 1.0


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, GOD, LESSER_GOD, MORTAL)
def test_teleport_needs_level_93(server) -> None:
    """Pins #268: a level-92 immortal does not have teleport, and a level-93 one teleports a mortal.

    command_interpreter (interpre.cpp) drops a command whose table row is above the speaker's
    level, so the level-92 immortal reads "Unrecognized command." and the mortal stays put; the
    level-93 immortal sends the same command and the mortal arrives in Arena East. No draw is
    on either path, and the affect-hours schedule keeps Arena East's target orc still.
    """
    lesser_god = server.login(LESSER_GOD.name)
    god = server.login(GOD.name)
    mortal = server.login(MORTAL.name)
    teleport_command = f"teleport {MORTAL.name.lower()} {TELEPORT_DESTINATION}"

    refused = lesser_god.command(teleport_command)
    assert refused.contains(UNRECOGNIZED_MARKER), refused.text
    assert TELEPORTED_MARKER_FORMAT.format(LESSER_GOD.name) not in mortal.drain(SETTLE_SECONDS)
    assert mortal.command("look").room_name() == "Arena Centre"

    granted = god.command(teleport_command)
    assert not granted.contains(UNRECOGNIZED_MARKER), granted.text
    teleported_marker = TELEPORTED_MARKER_FORMAT.format(GOD.name)
    teleported = mortal.expect([teleported_marker]) + mortal.drain(SETTLE_SECONDS)
    assert "Arena East" in teleported, teleported


@pytest.mark.characters(IMPLEMENTOR, AREA_GOD)
def test_shutdown_is_refused_at_level_95_and_the_server_stays_up(server) -> None:
    """Pins #358 and #359: a level-95 immortal does not have shutdown; the implementor does.

    Shutdown's row is LEVEL_GRGOD - 1 (96) in the repository (#359 left the level-93 row to a
    test-port deploy edit), so the level-95 immortal reads "Unrecognized command." and the
    server keeps answering. The implementor's `shutdown foo` reaches do_shutdown, which answers
    an unknown option without shutting anything down. Neither command is a real shutdown: the
    server exiting would fail the test.
    """
    implementor = server.login(IMPLEMENTOR.name)
    area_god = server.login(AREA_GOD.name)
    shutdown_command = f"shutdown {HARMLESS_SHUTDOWN_ARGUMENT}"

    refused = area_god.command(shutdown_command)
    assert refused.contains(UNRECOGNIZED_MARKER), refused.text
    assert UNKNOWN_OPTION_MARKER not in refused.text, refused.text
    assert area_god.command("look").room_name() == "Immortal Start"

    answered = implementor.command(shutdown_command)
    assert answered.contains(UNKNOWN_OPTION_MARKER), answered.text
    assert implementor.command("look").room_name() == "Immortal Start"
