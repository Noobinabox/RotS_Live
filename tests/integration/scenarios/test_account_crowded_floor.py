"""A crowded floor: a room's object list stops at 100 lines and counts the rest, so the people
in the room still reach the player."""

from __future__ import annotations

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character

pytestmark = [pytest.mark.scenario, pytest.mark.account_menu]

# A level-20 human warrior in the arena centre, who looks at the crowded floor.
ONLOOKER = Character(
    name="Testonlooker", race="human", sex="male", level=20, load_room=world.ROOM_ARENA_CENTRE, class_name="warrior"
)

# The leather cap (tests/integration/world/obj/11.obj): a plain object with no special procedure.
CAP_VNUM = 1137
# What `load obj` prints for each cap (do_load, act_wiz.cpp); it creates one object per command.
CAP_CREATED = "You create a leather cap."
# What `load obj` prints for a vnum the world lacks (do_load).
NO_SUCH_OBJECT = "There is no object with that number."
# What the implementor reads for each cap it drops (perform_drop, act_obj1.cpp).
CAP_DROPPED = "You drop a leather cap."
# The cap's line in a room's object list: its long description.
CAP_FLOOR_LINE = "A leather cap lies here."
# The most objects a room's floor lists (list_obj_to_char, act_info.cpp).
FLOOR_LIST_LIMIT = 100
# Caps put on the floor: five more than the list shows.
CAPS_DROPPED = FLOOR_LIST_LIMIT + 5
# The line that counts the objects past the limit (list_obj_to_char).
REMAINDER_LINE = "...and 5 more items are lying here."
# How the onlooker sees the implementor in the room (show_char_to_char, act_info.cpp): its name and
# rotstool's title, then the tag get_char_flag_line adds for a character big_brother forbids the
# onlooker to attack.
IMPLEMENTOR_IN_ROOM_LINE = "Testimm the Human (holy protection) is standing here."
# Seconds the onlooker reads before looking, so the drop messages are not taken for the look's reply.
SETTLE_SECONDS = 1.0


@pytest.mark.characters(IMPLEMENTOR, ONLOOKER)
def test_a_floor_with_more_than_100_objects_lists_100_then_a_remainder_line_then_the_people(server) -> None:
    """Pins #300: a room's floor lists at most 100 objects, then one line counting the rest, then the
    people in the room."""
    implementor = server.login(IMPLEMENTOR.name)
    onlooker = server.login(ONLOOKER.name)
    for _cap in range(CAPS_DROPPED):
        implementor.send_line(f"load obj {CAP_VNUM}")
        loaded = implementor.expect([CAP_CREATED, NO_SUCH_OBJECT])
        assert CAP_CREATED in loaded, loaded
    implementor.command(f"goto {world.ROOM_ARENA_CENTRE}")
    dropped = implementor.command("drop all")
    assert dropped.text.count(CAP_DROPPED) == CAPS_DROPPED, dropped.text
    onlooker.drain(SETTLE_SECONDS)

    looked = onlooker.command("look").text
    lines = [line.strip() for line in looked.splitlines() if line.strip()]
    cap_lines = [index for index, line in enumerate(lines) if line == CAP_FLOOR_LINE]
    assert len(cap_lines) == FLOOR_LIST_LIMIT, looked
    assert REMAINDER_LINE in lines, looked
    remainder_index = lines.index(REMAINDER_LINE)
    assert IMPLEMENTOR_IN_ROOM_LINE in lines, looked
    people_index = lines.index(IMPLEMENTOR_IN_ROOM_LINE)
    assert cap_lines[-1] < remainder_index < people_index, looked
