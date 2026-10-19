"""Virtual numbers of the test world's rooms that scenarios name (tests/integration/world)."""

from __future__ import annotations

# Room 1101, where implementors arrive.
ROOM_IMMORTAL_START = 1101
# Room 1130, the lit western end of the arena in zone 11.
ROOM_ARENA_WEST = 1130
# Room 1131, the middle of the arena in zone 11.
ROOM_ARENA_CENTRE = 1131
# Room 1132, the eastern end of the arena in zone 11, east of the centre.
ROOM_ARENA_EAST = 1132
# Room 1133, the arena's one DARK room, north of the centre in zone 11.
ROOM_DARK_CELL = 1133
# Room 1137, a DEATH room in zone 11 with no exits: whoever walks in dies (do_move, act_move.cpp).
ROOM_GALLOWS_PIT = 1137
# Room 1138, in zone 11, whose one exit leads east into the Gallows Pit.
ROOM_GALLOWS_APPROACH = 1138
# Room 1201, the only room of zone 12, whose map square is (3, 4).
ROOM_DISTANT_CELL = 1201
