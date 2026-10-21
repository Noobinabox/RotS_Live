# Zone 21: Harness minus-twenty zone

A zone twenty squares west of the arena on the world map, for summon's zone-distance scenarios:
its map square (-20, 0) is twenty squares from zone 11's (0, 0), the same distance as zone 19 on
the opposite side. Files: `wld/21.wld`, `zon/21.zon`. Vnums 2100 to 2199 (top 2199); no resets.
Zone 20 is unused, because guildmaster 2043 (`mob/guildmasters.mob`, `shared.md`) sits in its
vnums.

`python -m rots_harness.world_catalogue --zone 21` (from `tests/integration`) lists it.

## Rooms

| Vnum | Name | Constant | Notes |
| --- | --- | --- | --- |
| 2101 | Minus Twenty Cell | `ROOM_MINUS_TWENTY_CELL` in `test_summon.py` | the only room of zone 21; sector 0, no flags, no exits |
