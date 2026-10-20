"""Virtual numbers of the test world's rooms that scenarios name (tests/integration/world)."""

from __future__ import annotations

# Room 1101, where implementors arrive.
ROOM_IMMORTAL_START = 1101
# Room 1130, the lit western end of the arena in zone 11.
ROOM_ARENA_WEST = 1130
# Room 1131, the middle of the arena in zone 11.
ROOM_ARENA_CENTRE = 1131
# Room 1132, the eastern end of the arena in zone 11, east of the centre; zone 11 resets the target orc (mob 1130) here.
ROOM_ARENA_EAST = 1132
# Room 1133, the arena's one DARK room, north of the centre in zone 11.
ROOM_DARK_CELL = 1133
# Room 1137, a DEATH room in zone 11 with no exits: whoever walks in dies (do_move, act_move.cpp).
ROOM_GALLOWS_PIT = 1137
# Room 1138, in zone 11, whose one exit leads east into the Gallows Pit.
ROOM_GALLOWS_APPROACH = 1138
# Room 1190, the Uruk-hai idle room (mortal_idle_room, consts.cpp).
ROOM_URUK_IDLE = 1190
# Room 1201, the only room of zone 12, whose map square is (3, 4).
ROOM_DISTANT_CELL = 1201
# Room 1301, the lit bank hall in zone 13, where zone 13 resets the banker (mob 1301).
ROOM_BANK_HALL = 1301
# Room 1302, the lit trade hall in zone 13, where zone 13 resets the trader (mob 1302).
ROOM_TRADE_HALL = 1302
# Room 1303, the trader's store room in zone 13, with no exits; zone 13 stocks one leather cap.
ROOM_STORE_ROOM = 1303
# Room 1304, a DARK hall in zone 13, where zone 13 resets the dark banker (mob 1304).
ROOM_DARK_VAULT_HALL = 1304
# Room 1305 in zone 13, where zone 13 resets the morning banker (mob 1305), open hours 0 to 11.
ROOM_MORNING_BANK = 1305
# Room 1306 in zone 13, where zone 13 resets the evening banker (mob 1306), open hours 12 to 23.
ROOM_EVENING_BANK = 1306
# Room 1307 in zone 13, where zone 13 resets the odd trader (mob 1307).
ROOM_ODD_TRADE_HALL = 1307
# Room 1308, the odd trader's store room in zone 13, with no exits; zone 13 stocks a leather cap,
# an iron cap and a lead ingot.
ROOM_ODD_STORE_ROOM = 1308
# Room 1309 in zone 13, where zone 13 resets the broken banker (mob 1308) and the broken trader
# (mob 1309), each disabled by its options.
ROOM_BROKEN_MARKET = 1309
# Room 1701, zone 17's yard, whose north exit is a door (keyword door) to 1702.
ROOM_GATEHOUSE_YARD = 1701
# Room 1702, north of the yard, whose south exit is the same door seen from its side (keyword gate).
ROOM_BEYOND_THE_GATE = 1702
