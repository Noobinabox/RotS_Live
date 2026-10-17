---
name: rots-integration-scenario
description: Use when adding, changing or debugging a RotS integration scenario (tests/integration), when a behaviour players see can only be checked on a running server, when pinning today's behaviour before changing it, or when changing the integration harness, its schedules, control requests or ageland_harness options.
---

# Writing a RotS integration scenario

A scenario boots its own server on the test world and its declared characters, logs them in
and checks what they read. Work through the steps in order. Commands, formats and values are in
[harness-reference.md](harness-reference.md); changing the harness is in
[extending-the-harness.md](extending-the-harness.md).

## 1. Is a scenario the right test?

Only when the behaviour needs a running server: commands and their messages, several
characters online, or game time passing. A rule inside one function, on state a fixture can
build, is a GoogleTest test in `src/tests/`.

## 2. Read the behaviour in the code

- Follow the command from the interpreter (`interpre.cpp`) to every outcome, through the
  shared code on the way (casting in `do_cast`, skills, movement, combat).
- Note the order of the checks, each message's exact text and who receives it (`act()`'s
  `TO_CHAR`, `TO_VICT`, `TO_ROOM`), and which character, room or zone each check reads.
- List every random draw on that path: `number()`, `dice()` and whatever calls them. Check each
  roll's bounds: `number(0, 100) >= chance` still fails once in 101 at a chance of 100.
- List every input the behaviour reads and how it is controlled (table below).
- Read the numbers the outcome rests on in game with `stat player <name>`, after `restore`
  when `wizset` changed an ability (reference, "Abilities").

An input the harness cannot control needs a harness extension first, with the owner's
approval; never rely on luck.

| Input | Where the code reads it | Controllable today |
| --- | --- | --- |
| Light | `IS_DARK` (`utils.h`): the room's light count, its `DARK` flag, and sunlight in any sector but `SECT_INSIDE` and `SECT_CITY`. `CAN_SEE` (`utility.cpp`): blindness, `AFF_INFRARED`, `PRF_HOLYLIGHT`, and `AFF_MOONVISION` when `OUTSIDE` under moonlight | Yes: room flags in the test world, light objects the implementor loads, the implementor's `set holylight on` |
| Outdoors and sector | `OUTSIDE()` (`utils.h`) is true unless the room has the `INDOORS` flag, whatever its sector. `get_sun_level` (`weather.cpp`) is 0 in `SECT_INSIDE`, in a `DARK` room and at night; otherwise sector, sky, snow and `SHADOWY` set it | Yes: the room's flags and sector. Every test-world room is `SECT_INSIDE` without `INDOORS`: `OUTSIDE()` is true, but it is never sunlit |
| Time of day and sunlight | `weather_info.sunlight`, set by `initialize_weather` from the game clock, which `reset_time` (`db.cpp`) takes from the wall clock at boot; `weather_and_time` moves it each game hour | Partly: `advance_hours` moves the clock, but the starting hour depends on when the server boots. A known hour needs a harness extension |
| Weather, sky, moonlight | `weather_info`, changed each game hour by `weather_change` (called from `weather_and_time`) with `dice()` draws | No: needs a harness extension |
| Race | `other_side` (`handler.cpp`) for sides and name lookups; `PERS` (`utility.cpp`) star names; `race_affect` (`consts.cpp`): infravision for beorning, uruk-hai, orc, uruk-lhuth and olog-hai, moonvision for wood elf and haradrim; `EVIL_RACE` (`utils.h`) | Yes: the character's `race` |
| Power of Arda | `do_power_of_arda` (`limits.cpp`, from `fast_update` and on movement, `EVIL_RACE` only): Haradrim exempt, Olog-hai triple, scaled by the room's `get_sun_level`. `get_power_of_arda` (`utility.cpp`) lowers casting in `do_cast` (`spell_pa.cpp`) and combat values | Race and room, yes; sunlight only partly (above). In the test world as it stands `get_sun_level` is 0, so it never applies |

## 3. Decide the pin

- A **legacy pin** records today's behaviour before a change: written first, against unchanged
  code, and passing there. Its docstring says it records today's behaviour and becomes a strict
  xfail when the behaviour changes.
- The commit that changes the behaviour marks it
  `@pytest.mark.xfail(strict=True, reason="<the new behaviour>")`, keeping it as the record of
  the old behaviour; strict makes it fail the run if it ever passes again.
- A **new-behaviour pin** fails before the change and passes after it.
- A crash or memory-safety fix gets no legacy pin.

## 4. Make the outcome deterministic by construction

- Choose levels, abilities and rooms so no roll decides the outcome: drive a value to a clamp,
  or put one side of a comparison out of reach.
- A margin check (asserting from `stat` values that no roll can change the outcome) REQUIRES,
  for every case including the control, the comment the reference's "Margin checks" shows:
  the code's expression evaluated term by term, ending in the value passed.
- When a draw cannot be removed, mark the test `@pytest.mark.schedule("affect-hours")`. Then
  only the scenario's own commands and the hours it asks for draw, so the seed fixes the
  sequence. Name the remaining draw in the docstring.
- The seed alone is never why a test passes; step 9 runs other seeds to prove it.

## 5. Declare the characters

Use `@pytest.mark.characters(...)` (fields and limits in the reference). The implementor sets
what rotstool cannot (hunger, gold, items, abilities, saving throw); assert each reply, such as
`hunger set to 10.`. Declare `IMPLEMENTOR` whenever two characters are online. A command
naming a player on the other side matches its race keyword, never its name (reference, "Names,
sides and lookups"): place such characters with `load_room`, and use `stat player` and
`wizset player`.

## 6. Change the world only when it lacks what you need

Read the test world first (reference, "The test world"). Keep vnums ascending and list a new
file in its `index`. Boot after every change: a bad file shows only as a `SYSERR` or a server
that never reaches the login prompt. The world files, the
room constants in `rots_harness/world.py` and the markers in `pytest.ini` are scenario
territory; the rest of `tests/integration/` and `src/harness/` is the harness.

## 7. Assert what players see

- Match each message's exact text as that observer receives it (`PERS`): `someone` for an
  actor it cannot see, a star name such as `*an Orc*` across sides, and nothing from `act()`
  with `hide_invisible` set.
- Give `expect` every outcome the code can produce at that step, then assert which arrived, so
  a changed behaviour fails at once with the transcript rather than as a `SessionTimeout`.
- Check what the helpers return in odd states: in a dark room `look` prints
  `It is pitch black...` instead of the room name, and `room_name()` returns that line.
- Keep each pin to one behaviour. A control case (the same command succeeding once one input
  changes) must avoid the scenario's other quirks, or one break fails two pins.
- Advance time only with `server.advance_hours(n)`; read each hour's text with `expect` and
  `drain` before the next `command()`, which discards unread text.

## 8. Tag the group

Add `<name>: <what the group pins>` under `markers =` in `tests/integration/pytest.ini`, and set
`pytestmark = [pytest.mark.scenario, pytest.mark.<name>]`. `GROUP=<name>` then selects it.

## 9. Run it repeatedly

- While writing, run one test (reference, "Running"); run `make integration-unit` when the
  harness changed.
- `make integration PYTHON=.venv/bin/python GROUP=<name>` three times through the Docker
  launcher and three times through the local launcher (on a Mac, inside a container), then once
  each with `ROTS_IT_SEED=1` and `ROTS_IT_SEED=987654`. Every run must pass.
- Lock waits of many minutes are normal on a busy host: wait them out, never skip a required
  run. `DockerLockHeld` is not a test result; raise `ROTS_IT_LOCK_WAIT` and rerun (reference,
  "Launchers").
- On a failure, read the kept run directory (transcripts, `game.log`, `stdout.log`). Kept
  directories are git-ignored build output: remove only your run's, by name, or leave them.

## 10. Prove the pin bites

For each pin, break the behaviour it pins with an uncommitted edit in `src/`, rebuild, and run
its group. The pin must fail on an assertion about that behaviour, not on a timeout or setup
error. Then undo the edit with `git restore <files>`, rebuild and see it pass. Never commit
the break.

Commit only after the bite checks and the required runs. The Tests lines list only what ran;
amend them if a later run changes the result.

## Common mistakes

| Mistake | What to do instead |
| --- | --- |
| Working out a threshold or modifier from the formula | Read it with `stat player <name>` and assert the values the outcome rests on |
| Reading only the feature's function, so a roll in shared code (casting, combat) goes unseen | Trace from the interpreter; list every `number()` and `dice()` on the path |
| Passing on the default seed only | Run `ROTS_IT_SEED` at other values and both launchers |
| A margin with a term taken as 0 or neutral ("this term is 0 here") | Write the "Margin checks" comment for every case; each term's value names where the code computes it |
| `wizset ... int 40`, then reading the margin from the old current value | `restore <name>` after `wizset`, then `stat player` |
| Skipping the local-launcher runs because the lock wait is long | Wait them out, with a higher `ROTS_IT_LOCK_WAIT` if needed |
| Committing before the bite check and the runs | Commit last, with Tests lines for what actually ran |
| Believing a pin tells two behaviours apart | Break each one and watch the pin fail |
| An `expect` list holding only the expected line | List every outcome; assert which arrived |
| A control case that also crosses another pin's quirk | Change one input only; other inputs stay at values no break touches |
| A legacy pin whose docstring does not say what it is | State that it records today's behaviour and flips to a strict xfail on change |
| A guessed marker | Run once with `ROTS_IT_KEEP=1`, read the transcript, then find the string in `src/` |
| A fixed `time.sleep` or `drain(n)` as the wait | `expect` the event; `drain` only to settle after `expect` or `advance_hours` |
| `command()` after asynchronous text (hour messages, `The day has now begun.`) | `command()` returns at the first text ending in `>` or `]`; read pending text first, or `send_line` and `expect` the reply |
| A long reply read with `command()` (`stat` has lines ending in `]`) | `send_line`, then `expect` its last line and `drain` |
| Asserting exact hit points after several game hours | Regeneration runs in every hour: check once after the first hour, or assert a range |
| Trusting another connection's command right after a `teleport` | Commands from different connections are not ordered; `expect` the moved character's `has teleported you!` |
| `wizset <name> level` to change profession levels | It sets only the overall level, and `wizset ... prof` does nothing; declare the level and class |
| `wizset <name> room <vnum>` | It matches `roomflag`; set the room with `load_room`, or `teleport` a same-side character |
| Naming an orc, uruk, olog or haradrim player in `give`, `teleport`, `goto` or any targeted command | Across sides a name never matches, and `orc` can pick a mob; use `load_room`, the `player` forms, or `N.<race>` with an asserted reply |
| An immortal as the target of a mortal-only effect | Use a mortal; immortals are spared some effects (food poison, for one) |
| Two characters online without `IMPLEMENTOR` | Declare it; otherwise login fails with `You are already connected as` |
| A retry loop bounded by a count | Bound it by the mechanism's duration and fail with a message when the precondition is gone |
| Holding your own lock around `make integration` with the Docker launcher | Take none; the harness takes its own and waits for any other |
| Deleting kept run directories you did not create | Remove only the ones your run kept, by name; the rest are someone's evidence |
