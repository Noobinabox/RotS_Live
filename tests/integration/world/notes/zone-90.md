# Zone 90: Harness plus-twenty zone

A zone twenty squares east of the arena on the world map, for summon's zone-distance scenarios:
its map square (20, 0) is twenty squares from zone 11's (0, 0), the same distance as zone 91 on
the opposite side. Files: `wld/90.wld`, `zon/90.zon`. Vnums 9000 to 9099 (top 9099); no resets.
It and zone 91 are numbered in the 90s, clear of the low numbers new scenario areas take in turn.

`python -m rots_harness.world_catalogue --zone 90` (from `tests/integration`) lists it.

## Rooms

| Vnum | Name | Constant | Notes |
| --- | --- | --- | --- |
| 9001 | Plus Twenty Cell | `ROOM_PLUS_TWENTY_CELL` in `test_summon.py` | the only room of zone 90; sector 0, no flags, no exits |
