# Harness reference

The facts a scenario writer needs, cited by file and function. Paths are relative to the
repository root; `tests/integration/` is the harness root. When this file and the code disagree,
the code wins: fix this file in the same change.

## Running

Build the three programs once (32-bit Linux binaries; on a Mac, inside the container, as a
Docker job under the shared lock described in `/tmp/rots-docker-lock/README.txt`):

```bash
make configure && cmake --build build --target ageland ageland_harness rotstool
docker compose run --rm -T rots bash -lc 'make configure && cmake --build build --target ageland ageland_harness rotstool'
```

`make build` builds only `ageland`. A test with `@pytest.mark.schedule(...)` also needs
`bin/ageland_harness`; a missing program stops the run with the build command for its launcher
(`ServerLauncher.require_program` in `rots_harness/launcher.py`).

Create the virtual environment once: `python3 -m venv .venv && .venv/bin/pip install -r
tests/integration/requirements.txt`.

| What | Command (from the repository root) |
| --- | --- |
| The harness's own tests, no server | `make integration-unit PYTHON=.venv/bin/python` |
| Every scenario | `make integration PYTHON=.venv/bin/python` |
| One group | `make integration PYTHON=.venv/bin/python GROUP=poison` |
| Several groups | `make integration PYTHON=.venv/bin/python GROUP="boot or poison"` |
| One test, by part of its name | `.venv/bin/python -m pytest tests/integration/scenarios -q -m scenario -k tainted_bread` |
| One test, by node id | `.venv/bin/python -m pytest -q "tests/integration/scenarios/test_boot.py::test_an_implementor_logs_in_to_immortal_start"` |

`make integration` runs `$(PYTHON) -m pytest tests/integration/scenarios -q -m "scenario and
(<GROUP>)"`. Run directly, pytest finds the same `tests/integration/pytest.ini` and rootdir and
reads the same `ROTS_IT_*` variables, so the direct commands behave like `make integration`.
`-k` is not checked: a name that matches nothing prints `no tests collected (N deselected)`.
`-m` is checked: an unregistered name in it is a usage error listing the registered names
(`pytest_configure` in `conftest.py`, `rots_harness/groups.py`).

Run one harness session per checkout at a time: sessions in a checkout share the template
cache under `build/integration/libs/`.

### Launchers

| Launcher | Chosen by | How it runs the programs |
| --- | --- | --- |
| `docker` | default off Linux, or `ROTS_IT_LAUNCHER=docker` | `docker compose run --rm -T --name rots-it-<hex>` with this checkout's `rots` service, each server on a random loopback port. Takes the shared lock itself for the whole session (`rots_harness/dockerlock.py`) and waits while any other `*.lock` file is in the lock directory |
| `local` | default on Linux, or `ROTS_IT_LAUNCHER=local` | runs `bin/` directly; needs the i386 runtime (`libc6:i386`). Takes no lock |

Do not hold a lock of your own around `make integration` with the Docker launcher: the harness
waits for it, then fails.

**The local launcher on a Mac** runs inside a throwaway container. It is a Docker job outside
the harness, so first take a lock by hand as `/tmp/rots-docker-lock/README.txt` describes, and
remove it afterwards. From the repository root, with `ROTS_UID` and `ROTS_GID` exported
(`export ROTS_UID=$(id -u) ROTS_GID=$(id -g)`):

```bash
docker compose -f docker-compose.yml run --rm -T -u 0 rots bash -lc '
  ulimit -c 0
  sed -i "/bullseye-security/d" /etc/apt/sources.list
  apt-get -o Acquire::Retries=3 update -qq && apt-get install -y -qq python3 python3-venv >/dev/null
  python3 -m venv /tmp/harness-venv && /tmp/harness-venv/bin/pip install -q "pytest>=7.0"
  cd /rots && for run in 1 2 3; do ROTS_IT_LAUNCHER=local make integration PYTHON=/tmp/harness-venv/bin/python GROUP=<marker> || exit 1; done
  status=$?; chown -R "$(stat -c %u:%g /rots/Makefile)" /rots/build/integration; exit $status'
```

`-u 0` lets apt install Python; the final `chown` gives the run directories back to the host
user. `ulimit -c 0` matters: a core dump of a crashing i386 process under emulation hangs the
container.

### Environment variables

`validate_environment()` in `conftest.py` checks them all before any test; a bad value is a
usage error naming the variable.

| Variable | Default | Meaning |
| --- | --- | --- |
| `ROTS_IT_LAUNCHER` | `local` on Linux, `docker` elsewhere | `local` or `docker` |
| `ROTS_IT_BINARY` | `bin/ageland` | the server for tests without a schedule, relative to the repository root |
| `ROTS_IT_SEED` | `20261011` | the random seed for rotstool and every server (`--random-seed`) |
| `ROTS_IT_KEEP` | unset | `1` keeps every run directory and template lib |
| `ROTS_IT_LOCK_WAIT` | `600` | seconds the Docker launcher waits for the lock |
| `ROTS_IT_DOCKER_LOCK_DIR` | `/tmp/rots-docker-lock` | the shared lock directory |
| `ROTS_IT_HOUR_TIMEOUT` | `10.0` | seconds `advance_hours` allows per game hour; the wait is `hours * timeout + 10` |

The local launcher passes the server a clean environment, forwarding only `ASAN_OPTIONS`,
`LSAN_OPTIONS`, `UBSAN_OPTIONS` and `ASAN_SYMBOLIZER_PATH`.

### Run directories

Each test gets `build/integration/runs/<12 hex>/`:

| File | Contents |
| --- | --- |
| `test-id.txt` | the test's node id |
| `lib/` | the test's own copy of the template lib |
| `game.log` | the server's stderr, and its stdout too when the test has no schedule |
| `stdout.log` | a schedule server's ordinary stdout lines; `harness: ` replies are not written here |
| `<lowercase name>.txt` | one transcript per character, appended on every read, colour codes removed |

A passing test's directory is deleted. It is kept when `ROTS_IT_KEEP=1`, when any test in the
session has failed, when the server's checks fail, when the run is interrupted, or when setup
failed after the lib was copied. Kept directories are listed at the end of the run under "kept
run directories". Template libs live in `build/integration/libs/<key>/` and are deleted at the
end of the session unless `ROTS_IT_KEEP=1`.

### What fails a test besides its assertions

`CrashMonitor` (`rots_harness/crashmonitor.py`) reads `game.log` and the exit status: a
sanitizer marker, `Error: signal`, any `SYSERR` line (none is allowed today), the server exiting
during the test, and after the stop a non-zero status or a status 0 without the
`Received SIGHUP, SIGINT, or SIGTERM` line. A session that does not answer `quit` also fails the
test.

## Characters

### Declaring them

`Character` (`rots_harness/characters.py`) is a frozen dataclass; `to_spec()` turns it into
rotstool's fixture spec, and `bin/rotstool fixtures write` validates it with the server's own
rules (`src/tools/rotstool/fixture_spec_parser.cpp`). A refusal fails the test with rotstool's
message.

| Field | Accepts |
| --- | --- |
| `name` | 3 to 12 ASCII letters, unique in the test; stored with a capital first letter |
| `race` | `human`, `dwarf`, `wood elf`, `hobbit`, `uruk-hai`, `orc`, `uruk-lhuth`, `beorning`, `olog-hai`, `haradrim` |
| `sex` | `male`, `female` |
| `level` | 1 to 100, reached through the game's own levelling, so abilities, hit points, mana and moves are those of a real character of that level |
| `class_name` or `points` | exactly one. `class_name`: `mystic`, `ranger`, `warrior`, `mage`, `conjurer`, `wizard`, `healer`, `swashbuckler`, `barbarian`, `adventurer`. `points`: `ProfessionPoints(mage, mystic, ranger, warrior)`, each 0 to 150 and 150 in total at most |
| `load_room` | a room vnum; not checked against the world |
| `skills` | `(("skill_key", practices), ...)`: the key is the skill name in lower case with other characters as `_` (`magic_missile`); practices 1 to 255 are practice sessions, not a percentage |
| `hit`, `mana`, `move` | current points, 1 to the levelled maximum; above it the whole write fails |

Shared definitions: `IMPLEMENTOR` (`Testimm`, human male warrior, level 100, starts in 1101) and
`custom_mage()` (`Testmage`, human female, level 30, points 70/40/20/20, starts in 1131). A
test's characters all share one account, in marker order. The game allows one character online
per account unless the account owns one above level 91, so a test with two characters online
declares `IMPLEMENTOR`.

### What rotstool cannot set, and the in-game command that does

The implementor sets these after logging in. `wizset` (`do_wizset`, `act_wiz.cpp`) matches its
field by prefix, takes the first match, clamps the value, and replies `<Name>'s <field> set to
<value>.` with the clamped value: assert that reply.

| Need | Command (as the implementor) | Notes |
| --- | --- | --- |
| Hunger, thirst | `wizset player <name> hunger 10` | 0 to 24, or `off` |
| Gold | `wizset player <name> gold 100` | |
| Abilities | `wizset player <name> str 18` (`int`, `will`, `dex`, `con`, `lea`) | 0 to 40; sets the constant ability, then recalculates |
| Saving throw | `wizset player <name> saving_throw 5` | -100 to 100 |
| Hit points | `wizset player <name> hit 50`; `maxhit` sets the constant behind the maximum | `restore <name>` refills to the maximum |
| Items | `load obj <vnum>` (into the implementor's inventory), then `give <keyword> <name>` | |
| Mobs | `load mob <vnum>` (into the implementor's room) | |
| Position in the world | `teleport <name> <vnum>`, `transfer <name>`, `goto <vnum>` | |
| Profession levels | none | `wizset ... level` sets only the overall level and `wizset ... prof` does nothing; choose `level` and `class_name` or `points` |
| Affects, flags | cast the spell, or wear an object that sets it | |

### Reading a character in game

The implementor's `stat player <name>` (`do_stat` in `act_info.cpp`, then `do_wizstat` and
`do_stat_character` in `act_wiz.cpp`) prints, among other lines:

```text
Class levels: Mag:0 Cle:0 Ran:0 War:20,  mini-level: ...
Spec:(0) ..., Lev: [20], XP: [...], Align: [...]
Str:[cur/base/const] Int:[...] Wil:[...] Dex:[...] Con: [...] Lea:[...]
HP :[cur/max+gain(bonus)]  Stamina :[...]  Move :[...] ...
Encumbrance n, Leg_encu n, Perception n, Willpower n,
 OB: ..., parry: ..., dodge: ..., c_parry n,  Saving throws n,
Hunger: n, Thirst: n, Drunk: n, Att.Level: n
AFF: <affect bits>
RES: ... VUL: ...
SPL: (  4hr) <affect name> ...
```

`Cle` is the mystic level. The `SPL:` lines (one per affect) come last, after `VUL:`. Several
lines end in `]`, which `command()` takes for a prompt, so read the reply with
`send_line(f"stat player {name}")`, then `expect(["VUL:"])` and `drain(1.0)`. Parse with, for
example, `re.search(r"Saving throws (-?\d+),", text)` and
`re.search(r"Class levels: Mag:(\d+) Cle:(\d+) Ran:(\d+) War:(\d+)", text)`.

`stat player` finds the character by exact name whichever side it is on, but only if the
implementor can see it: for a character in a dark room, send `set holylight on` first (reply
`Holylight mode is now on.`). `stat` with no argument shows the speaker's own abilities, and
`stat mob <keyword>` shows a mob.

### Names, sides and lookups

`IMPLEMENTOR` is human. `get_char_vis` (`handler.cpp`), used by `goto <name>`, `teleport`,
`transfer`, `stat <name>` and `wizset <name>`, matches a character on the other side
(`other_side`) by its race keyword (`orc`, `uruk`, `olog`, ...) instead of its name, and only
when the implementor can see it. The `player` forms (`stat player`, `wizset player`) go through
`get_player_vis` and match the exact name. `restore <name>` matches the name anywhere.

What an observer reads for a character is `PERS` (`utility.cpp`): the name, a star name such as
`*an Orc*` across sides, or `someone` when the observer cannot see it. `act()` (`comm.cpp`) with
`hide_invisible` set sends nothing to an observer who cannot see the actor.

## Session API

`server` (the fixture in `conftest.py`) is a `HarnessServer`:

| Member | Use |
| --- | --- |
| `login(name)` | logs the character in and returns its `GameSession`; it is quit after the test |
| `advance_hours(n)` | runs `n` game hours on a schedule server and returns when the server acknowledges them; fails the test without `@pytest.mark.schedule` |
| `run_dir`, `lib_dir` | the run directory and the test's lib |

`GameSession` (`rots_harness/session.py`):

| Method | Behaviour |
| --- | --- |
| `command(text, timeout=8.0)` | drains unread text for 0.1 s and discards it, sends `text`, returns a `Transcript` of everything up to the first read whose text ends in `>` or `]` |
| `expect(markers, timeout=8.0)` | returns all unread text once any marker appears in it |
| `drain(timeout=0.5)` | reads for `timeout` seconds and returns the unread text |
| `send_line(line)` | sends a line without waiting |
| `everything` | all text received so far |

`Transcript` has `.text`, `.contains(marker)` and `.room_name()`, the first non-empty line up to
two spaces. A timeout raises `SessionTimeout` (an `AssertionError`) quoting the last 1500
characters. A refused login raises `LoginRefused` at once with the game's words.

What helpers return in odd states (`do_look`, `act_info.cpp`): a character who cannot see gets
`It is pitch black...` instead of the room, so `room_name()` returns `It is pitch black...`;
a sleeping one gets `You can't see anything, you're sleeping!`; a blind one gets
`You can't see a damned thing, you're blinded!`.

One example, abridged from `scenarios/test_poison.py`:

```python
pytestmark = [pytest.mark.scenario, pytest.mark.poison]

# What the mortal reads on each poison tick.
POISON_TICK_MARKER = "You writhe in pain as poison courses through your veins."
# A level-20 human warrior in the arena: a mortal, since immortals are not poisoned by food.
MORTAL = Character(name="Testeater", race="human", sex="male", level=20,
                   load_room=world.ROOM_ARENA_CENTRE, class_name="warrior")


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, MORTAL)
def test_tainted_bread_poisons_a_mortal_once_per_game_hour_then_wears_off(server) -> None:
    implementor = server.login(IMPLEMENTOR.name)
    mortal = server.login(MORTAL.name)
    set_hunger = implementor.command("wizset testeater hunger 10")
    assert set_hunger.contains("hunger set to 10."), set_hunger.text
    implementor.command("goto testeater")
    assert implementor.command("load obj 1139").contains("You create")
    assert implementor.command("give bread testeater").contains("You give")
    assert mortal.command("eat bread").contains("Oops, that tasted rather strange!")
    server.advance_hours(1)
    first_hour = mortal.expect([POISON_TICK_MARKER]) + mortal.drain(1.0)
    assert first_hour.count(POISON_TICK_MARKER) == 1, first_hour
```

### Timing rules

- Commands from different connections are not ordered by when they were sent: the game loop
  (`comm.cpp`) takes one command per connection per pass, in connection order. Wait for one
  character's reply before relying on its effect from another connection.
- Text can arrive between commands: affect messages each hour, and in any room without
  `INDOORS` or `DARK` (most test-world rooms) the sun and moon lines `send_to_outdoor` sends
  (`weather.cpp`): `The day has now begun.`, `Night falls over the land.`,
  `The <phase> moon shows in the sky.`, `The <phase> moon goes off the sky.`. Read it with
  `expect`/`drain` before the next `command()`.
- A command with a delay (`WAIT_STATE_FULL` or `WAIT_STATE_BRIEF`, `utils.h`; casting and
  `search` use them) sets `AFF_WAITING`: the game takes no further command from that character until the delay ends,
  and the outcome can arrive after the reply `command()` returns. `expect` the outcome.
- After `advance_hours` returns, the server has run the passes; `drain(1.0)` (or `expect` on
  the hour's marker, then `drain`) collects text already sent.
- Regeneration (`fast_update`) runs inside every advanced hour, and the hourly update runs
  before it in the same pass: assert hit points once, after the first hour, or as a range.

## Markers and groups

`tests/integration/pytest.ini` runs under `--strict-markers`:

| Marker | Effect |
| --- | --- |
| `scenario` | boots its own server through the `server` fixture; `make integration` always selects it |
| `characters(*characters)` | the test's characters; a test using `server` without it fails |
| `schedule(name)` | runs `bin/ageland_harness --schedule <name>`; game time advances only through `advance_hours` |
| `boot`, `poison` | groups, with no behaviour of their own |

A new group is one line `<name>: <description>` under `markers =` (the name must start the
line), plus `pytest.mark.<name>` in the scenario's `pytestmark`. `rots_harness/groups.py` and
the Makefile need no change.

## Time control

Without a schedule the server runs on the wall clock: mob activity, combat, zone resets and the
hourly update all happen in real time.

With `@pytest.mark.schedule("affect-hours")`, the only schedule today, the harness binary
starts and writes `harness: ready schedule=affect-hours hours=0` to stdout; the fixture waits
for it. World updates then run only when granted. `advance_hours(n)` writes `advance-hours n`
(1 to 1000) to the server's stdin; the reply is `harness: done advance-hours n hours=<total>` or
`harness: error <reason>`, which raises `TimeControlError`.

| An advanced hour runs | It never runs |
| --- | --- |
| 19 fast-update passes, then one pass with the hourly update (`weather_and_time`, `point_update`, `stat_update`) and the fast update (`fast_update`, `affect_update`) | zone resets, mob activity, combat (`perform_violence`), skill timers |

So a scenario needing combat, wandering mobs or resets has no schedule today: it uses a plain
server, or a new schedule (extending-the-harness.md).

### Randomness

Every draw goes through one `std::rand()` stream (`number()` and `dice()`, `utility.cpp`),
seeded from `--random-seed` (`ROTS_IT_SEED`). On a plain server mob activity and combat draw on
the wall clock, so the seed does not make a run repeat. Under `affect-hours`, between requests
only the scenario's commands draw, and each hour draws in the same order (`weather_change`
among others), so the same commands give the same sequence.

## The test world

`LibTemplateBuilder.build` (`rots_harness/libbuilder.py`) copies `tests/integration/world/<cat>`
for each of `wld mob obj zon shp scr mdl` into the lib; the real `lib/world` is not used. Adding
a category means editing `WORLD_CATEGORIES`.

### Zones

| Zone | File | Name | Map (x, y) | Rooms | Resets |
| --- | --- | --- | --- | --- | --- |
| 11 | `zon/11.zon` | Harness arena | (0, 0) | 1101-1199 | target orc (mob 1130) to 1132; harness snake (mob 1131) to 1134 |
| 12 | `zon/12.zon` | Harness distant zone | (3, 4) | 1201-1299 | none |

### Rooms

Every room is in its zone with sector 0 (`SECT_INSIDE`) and level 0. The table lists the rooms
with no flags; read the header line of each room in `wld/11.wld` and `wld/12.wld` before
relying on what a room is, since its flags and sector decide what the code reads.

| Vnum | Name | Exits |
| --- | --- | --- |
| 1101 | Immortal Start (`world.ROOM_IMMORTAL_START`) | none |
| 1102 | Immortal Idle | none |
| 1110 | Frozen Room | none |
| 1129 | Olog-hai Start | none |
| 1130 | Arena West | east 1131, down 1136 |
| 1131 | Arena Centre (`world.ROOM_ARENA_CENTRE`) | north, east 1132, south 1134, west 1130 |
| 1132 | Arena East | west 1131 |
| 1134 | Corridor One | north 1131, south 1135 |
| 1135 | Corridor Two | north 1134 |
| 1136 | Crevice Floor | up 1130 |
| 1151 | Retirement Home | none |
| 1152 | Lost and Found | none; where a character with a bad load room arrives |
| 1160 | Human Start | none |
| 1170 | Wood-elf Start | none |
| 1184 | Beorning Start | none |
| 1190 | Uruk Idle | none |
| 1191 | Orc Idle | none |
| 1201 | Distant Cell (zone 12) | none |

### Mobs and objects

`mob/11.mob` holds mobs 1130 to 1135: the target orc, the harness snake (its bite poisons; act
flags `3` include `MOB_SPEC`), and a bystander, a brute, a pet and a hardy target orc for combat
scenarios. `mob/guildmasters.mob` copies six real guildmasters at their real vnums (1503, 2043,
4601, 10003, 13600, 32200) so `spec_ass.cpp` binds the guild special to them.

| Object | Keyword | What it is |
| --- | --- | --- |
| 1130 | `token` | harness token; its vnum is one `spec_ass.cpp` assigns a board to |
| 1136 | `bag` | leather bag, a container |
| 1137 | `cap` | leather cap, head armour |
| 1138 | `amulet` | sickly amulet, worn on the neck; its `A 28 11` line (`APPLY_BITVECTOR`, bit 11) sets `AFF_POISON` on the wearer with no poison affect behind it |
| 1139 | `bread` | tainted bread: food, fill 2, poisoned |

There are no shops (`shp/index` is `$` alone); `scr/11.scr` and `mdl/11.mdl` are stubs.

### File formats

Each category directory has an `index` listing its files one per line, ending with a line `$`
(`index_boot`, `db.cpp`). A file not in its `index` is not loaded.

A room (`load_rooms`, `db.cpp`):

```text
#1131
Arena Centre~
The centre of the harness arena. ...
~
11 0 0 0
D0
~
~
0 0 <to room> 5
S
```

- The header line is `<zone> <flags> <sector> <level>`. Flags are bits from `structs.h`,
  among them `DARK` 1, `DEATH` 2, `NO_MOB` 4, `INDOORS` 8, `NORIDE` 16, `SHADOWY` 64, `NO_MAGIC` 128,
  `TUNNEL` 256, `PRIVATE` 512, `GODROOM` 1024, `PEACEROOM` 32768, `NO_TELEPORT` 65536. Sectors:
  0 inside, 1 city, 2 field, 3 forest, 4 hills, 5 mountain, 6 water (swim), 7 water (no swim),
  8 underwater, 9 road, 10 crack, 11 dense forest, 12 swamp.
- An exit is `D<direction>` (0 north, 1 east, 2 south, 3 west, 4 up, 5 down), its description
  `~`, its keyword `~`, then `<exit flags> <key> <to room> <width>` (`setup_dir`).
- `S` ends the room; the file ends with `#99999` and `$~`.
- Rooms must ascend and sit inside a zone's range: a room outside every zone stops the boot
  (`Room <n> is outside of any zone.`) without a `SYSERR`.

A zone (`load_zones`, `zone.cpp`): `#<n>`, name `~`, description `~`, map `~`, an owner list
ending in `0`, `<symbol> <x> <y> <level>`, the top room, the lifespan, the reset mode, then reset
rows ending with `S`. An `M` row is `M <if> <mob> <room> <max in world> <percent> <difficulty>
<max existing> <trophy> <comment>`.

Mobs (`load_mobiles`) and objects (`load_objects`) in `db.cpp`: copy an existing entry and read
the loader before changing a field you do not know. Vnums must ascend across the files of the
index as well as within each, because `real_mobile` and `real_object` binary-search.

### Adding to the world

- A room: append it to `wld/11.wld` before `#99999`, between 1101 and 1199 in vnum order, or
  add `wld/<n>.wld` with a `zon/<n>.zon` and list both in their `index` files. Add a constant
  to `rots_harness/world.py` when a scenario names it.
- A mob: append to `mob/11.mob` in vnum order. A special needs the `MOB_SPEC` act flag as well
  as the program number. Place it with a zone reset row or `load mob`.
- An object: append to `obj/11.obj`; avoid vnums `spec_ass.cpp` passes to `ASSIGNOBJ`.
- Boot after every change: the only check is `test_boot.py` and every other scenario booting,
  failing on any `SYSERR`, crash or missing login prompt.

## Scenario territory and harness territory

| Scenario territory | Harness territory (extending-the-harness.md) |
| --- | --- |
| `tests/integration/scenarios/test_<group>.py` | `tests/integration/conftest.py` |
| `tests/integration/world/` | `tests/integration/rots_harness/` other than `world.py` |
| room constants in `rots_harness/world.py` | `tests/integration/unit/` |
| a marker line in `tests/integration/pytest.ini` | `src/harness/`, `src/tools/rotstool/` |

A helper used by one scenario stays in its scenario file, as `hit_points` does in
`test_poison.py`.
