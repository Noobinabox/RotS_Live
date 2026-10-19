"""Virtual numbers of the test world's rooms that scenarios name (tests/integration/world)."""

from __future__ import annotations

# Room 1101, where implementors arrive.
ROOM_IMMORTAL_START = 1101
# Room 1130, the lit western end of the arena in zone 11.
ROOM_ARENA_WEST = 1130
# Room 1131, the middle of the arena in zone 11.
ROOM_ARENA_CENTRE = 1131
# Room 1133, the arena's one DARK room, north of the centre in zone 11.
ROOM_DARK_CELL = 1133
# Room 1201, the only room of zone 12, whose map square is (3, 4).
ROOM_DISTANT_CELL = 1201
# Room 1801, zone 18's PEACEROOM boar pen, where the zone resets one pen boar (mob 1450).
ROOM_BOAR_PEN = 1801
# Room 1802, the pen gate east of the boar pen in zone 18, with a one-way exit east to the
# whistle hub.
ROOM_PEN_GATE = 1802
# Room 1803, zone 18's whistle hub: its six exits lead to the spokes 1804 to 1809, each a dead
# end back to the hub, and it has no exit back to the pen gate.
ROOM_WHISTLE_HUB = 1803
# Room 1804, the whistle hub's north spoke, the first of its six spokes.
ROOM_NORTH_SPOKE = 1804
# Room 1805, the whistle hub's east spoke.
ROOM_EAST_SPOKE = 1805
# Room 1809, the whistle hub's lower spoke, the last of its six spokes.
ROOM_LOWER_SPOKE = 1809
