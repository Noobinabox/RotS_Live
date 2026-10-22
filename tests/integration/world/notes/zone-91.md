# Zone 91: Harness minus-twenty zone

A zone twenty squares west of the arena on the world map, for summon's zone-distance scenarios:
its map square (-20, 0) is twenty squares from zone 11's (0, 0), the same distance as zone 90 on
the opposite side. Files: `wld/91.wld`, `zon/91.zon`. Vnums 9100 to 9199 (top 9199); no resets.

`python -m rots_harness.world_catalogue --zone 91` (from `tests/integration`) lists it.

## Rooms

| Vnum | Name | Constant | Notes |
| --- | --- | --- | --- |
| 9101 | Minus Twenty Cell | `ROOM_MINUS_TWENTY_CELL` in `test_summon.py` | the only room of zone 91; sector 0, no flags, no exits |
