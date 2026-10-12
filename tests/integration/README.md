# Integration harness

## What it is

Each scenario boots its own `ageland` server on a throwaway lib, logs characters in over
telnet, and checks what they see. The lib is built from the small test world in
`tests/integration/world` plus an account and characters that `bin/rotstool` writes from the
characters the scenario declares. Of a developer's `lib/`, only `lib/text` and the
git-tracked `lib/misc` files are copied in.

A scenario fails when the server shows trouble, even if its own checks pass:

- a crash marker in the server log (a sanitizer report or the `Error: signal` line the
  server prints when it catches a fatal signal);
- a `SYSERR` line the harness does not expect;
- the server exiting during the test, or, when it is stopped, exiting with a non-zero status or
  without logging its shutdown notice.

`rots_harness/` holds the pieces (telnet, characters, launchers, the lib builder, crash
checks, sessions); `conftest.py` turns them into pytest fixtures; `scenarios/` holds the
scenarios; `unit/` tests the harness itself without a server.

## Setup

1. Build the server and `rotstool`:

   ```bash
   make configure && cmake --build build --target ageland rotstool
   ```

   The binaries are 32-bit Linux programs. On a Mac, build them inside the i386 container, for
   example
   `docker compose run --rm -T rots bash -lc 'make configure && cmake --build build --target ageland rotstool'`.

2. Create the virtual environment once, from the repository root (Python 3.10 or later):

   ```bash
   python3 -m venv .venv && .venv/bin/pip install -r tests/integration/requirements.txt
   ```

## Running

```bash
make integration-unit PYTHON=.venv/bin/python           # the harness's own tests; no server
make integration PYTHON=.venv/bin/python                # every scenario
make integration PYTHON=.venv/bin/python GROUP=boot     # one group
```

A group is a marker registered in `tests/integration/pytest.ini` (today `boot`). A name that no
marker registers stops the run with a usage error that lists the registered names, so a
misspelt group never passes by selecting nothing.

## Launchers

The harness runs the i386 programs in one of two ways:

- `local` runs them directly. It is the default on Linux.
- `docker` runs each server and each `rotstool` call through `docker compose run` with this
  checkout's `rots` service. It is the default everywhere else. Every container is named
  `rots-it-...` and publishes the server on a random loopback port, so the service's fixed
  `rots` container name and host port 1024 are never used. Everything a container touches
  lives under the checkout, which the service mounts at `/rots`.

Sanitizer variables (`ASAN_OPTIONS`, `LSAN_OPTIONS`, `UBSAN_OPTIONS`, `ASAN_SYMBOLIZER_PATH`)
are forwarded to the server by the `local` launcher only.

Environment variables:

| Variable | Default | Meaning |
| --- | --- | --- |
| `ROTS_IT_LAUNCHER` | `local` on Linux, `docker` elsewhere | `local` or `docker` |
| `ROTS_IT_BINARY` | `bin/ageland` | the server binary, relative to the repository root |
| `ROTS_IT_SEED` | `20261011` | the random seed for `rotstool` and every server |
| `ROTS_IT_KEEP` | unset | `1` keeps every run directory and template |
| `ROTS_IT_LOCK_WAIT` | `600` | seconds to wait for the Docker lock |
| `ROTS_IT_DOCKER_LOCK_DIR` | `/tmp/rots-docker-lock` | the shared Docker lock directory |

## Shared Docker lock

Docker jobs on one host share the lock directory `/tmp/rots-docker-lock`: a job runs only while
no other job's `.lock` file is there. With the `docker` launcher the harness takes the lock
itself as `harness-it-<pid>-<id>.lock`, holds it for the whole pytest session, and removes it
at the end. While another job holds the lock it waits up to 10 minutes (`ROTS_IT_LOCK_WAIT`),
printing which lock files hold it, then fails.

Ctrl-C, SIGTERM and SIGHUP all end the run the same way: the harness stops the server it
started and removes its lock file. A `harness-it-<pid>-<id>.lock` whose process is no longer
running (a run that was killed outright) is removed automatically, and the removal is printed.
The harness never removes any other lock file; judge a stale one by the convention in the lock
directory's `README.txt`.

Do not hold a lock of your own around `make integration`: the harness would wait for it and
then fail. If the directory does not exist, create it (`mkdir -p /tmp/rots-docker-lock`). The
`local` launcher takes no lock.

## Run directories

Each test gets `build/integration/runs/<id>/` holding:

- `lib/`, the test's own copy of the lib;
- `game.log`, the server's output;
- one transcript per character (`testimm.txt` for `Testimm`), the text the character read.

A passing test's directory is deleted. It is kept, and its path printed, when the lib or
server fails to set up after the lib was copied in, when any test in the session has failed,
when the server's checks fail, or with `ROTS_IT_KEEP=1`. Template libs, one per character set,
are built once per session under `build/integration/libs/` and deleted at the end unless
`ROTS_IT_KEEP=1`.
Containers use `build/integration/tmp` as their `TMPDIR`.

## Writing a scenario

Mark the test `scenario` plus its group, declare its characters with
`@pytest.mark.characters(...)` using the definitions in `rots_harness.characters`, and log in
through the `server` fixture:

```python
import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, custom_mage

pytestmark = [pytest.mark.scenario, pytest.mark.boot]

MAGE = custom_mage(level=30, load_room=world.ROOM_ARENA_CENTRE)


@pytest.mark.characters(IMPLEMENTOR, MAGE)
def test_the_mage_starts_in_the_arena(server) -> None:
    implementor = server.login(IMPLEMENTOR.name)
    mage = server.login(MAGE.name)
    assert mage.command("look").room_name() == "Arena Centre"
```

All of a test's characters belong to one harness account, listed in marker order.
`server.login(name)` picks the character by name from the account's character list, so the
order does not matter to logging in. It returns a session whose `command(text)` returns the
command's output; the
fixture quits every session after the test. The game lets an account have only one character
online unless the account owns a character above level 91, so a test that logs in two
characters at once also declares one above that level, such as `IMPLEMENTOR` (level 100).

A new group needs a line under `markers` in `pytest.ini`.
