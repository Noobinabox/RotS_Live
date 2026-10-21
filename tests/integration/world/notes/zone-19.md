# Zone 19: Harness plus-twenty zone

A zone twenty squares east of the arena on the world map, for summon's zone-distance scenarios:
its map square (20, 0) is twenty squares from zone 11's (0, 0), the same distance as zone 21 on
the opposite side. Files: `wld/19.wld`, `zon/19.zon`. Vnums 1900 to 1999 (top 1999); no resets.

`python -m rots_harness.world_catalogue --zone 19` (from `tests/integration`) lists it.

## Rooms

| Vnum | Name | Constant | Notes |
| --- | --- | --- | --- |
| 1901 | Plus Twenty Cell | `ROOM_PLUS_TWENTY_CELL` in `test_summon.py` | the only room of zone 19; sector 0, no flags, no exits |
