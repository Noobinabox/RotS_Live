"""Fixtures that give each scenario a fresh server built from the characters its marker declares."""

from __future__ import annotations

import os
import platform
import shutil
import uuid
from dataclasses import dataclass, field
from pathlib import Path

import pytest

from rots_harness import groups
from rots_harness.characters import CharacterSet
from rots_harness.crashmonitor import CrashMonitor
from rots_harness.dockerlock import DEFAULT_LOCK_DIR, DEFAULT_WAIT_SECONDS, DockerLock
from rots_harness.launcher import DockerComposeLauncher, LocalProcessLauncher, ServerHandle, ServerLauncher, allocate_free_port
from rots_harness.libbuilder import LibTemplateBuilder, RunLibProvider
from rots_harness.retention import keep_run_directory
from rots_harness.session import GameSession, end_sessions

# tests/integration: this file, pytest.ini and the test world.
INTEGRATION_ROOT = Path(__file__).resolve().parent
# The checkout whose bin/ and lib/ the harness uses; containers mount it at /rots.
REPO_ROOT = INTEGRATION_ROOT.parents[1]
# One directory per test: its lib, game.log and one transcript per character.
RUNS_ROOT = REPO_ROOT / "build" / "integration" / "runs"
# One template lib per character set, built once per session.
TEMPLATES_ROOT = REPO_ROOT / "build" / "integration" / "libs"
# The seed rotstool and every server use unless ROTS_IT_SEED gives another.
DEFAULT_SEED = 20261011


def pytest_configure(config: pytest.Config) -> None:
    """Stops the run with a usage error when the -m expression names an unregistered marker."""
    expression = config.getoption("markexpr", default="") or ""
    unknown = groups.unknown_marker_names(expression, groups.registered_marker_names(config.getini("markers")))
    if unknown:
        registered = ", ".join(groups.registered_marker_names(config.getini("markers")))
        raise pytest.UsageError(f"unknown group or marker {', '.join(unknown)} in -m {expression!r}; registered: {registered}")


def launcher_mode() -> str:
    """'local' or 'docker' from ROTS_IT_LAUNCHER, defaulting to local on Linux and Docker elsewhere."""
    mode = os.environ.get("ROTS_IT_LAUNCHER") or ("local" if platform.system() == "Linux" else "docker")
    if mode not in ("local", "docker"):
        raise pytest.UsageError(f"ROTS_IT_LAUNCHER must be 'local' or 'docker', not {mode!r}")
    return mode


def keep_requested() -> bool:
    """Whether ROTS_IT_KEEP=1 asks to keep every run directory and template."""
    return os.environ.get("ROTS_IT_KEEP") == "1"


def session_seed() -> int:
    """The random seed for rotstool and every server, from ROTS_IT_SEED or the default."""
    return int(os.environ.get("ROTS_IT_SEED", DEFAULT_SEED))


@dataclass
class HarnessServer:
    """One test's running server, its lib and run directory, and the sessions logged in to it."""

    # The started server; the server fixture stops it after the test.
    handle: ServerHandle
    # This test's own copy of the character set's template lib.
    lib_dir: Path
    # Holds the lib, game.log and one transcript per character.
    run_dir: Path
    # The characters the test's marker declared; login() looks names up here.
    character_set: CharacterSet
    # Reads the server's log and exit state for crash evidence.
    monitor: CrashMonitor
    # Every session login() opened, quit by the server fixture after the test.
    sessions: list[GameSession] = field(default_factory=list)
    # Set by the crash checks; keeps the run directory.
    crash_detected: bool = False

    def login(self, name: str) -> GameSession:
        """Logs the named character in and returns its session; the fixture quits it after the test."""
        character = self.character_set.character(name)
        game_session = GameSession(
            self.handle, character.name, self.character_set.character_number(name),
            self.character_set.email, self.character_set.password, self.run_dir,
        )
        try:
            game_session.login()
        except BaseException:
            game_session.close()
            raise
        self.sessions.append(game_session)
        return game_session


@pytest.fixture(scope="session")
def harness_launcher() -> ServerLauncher:
    """The launcher ROTS_IT_LAUNCHER selects, running ROTS_IT_BINARY (default bin/ageland)."""
    binary_relative = os.environ.get("ROTS_IT_BINARY", "bin/ageland")
    if launcher_mode() == "local":
        return LocalProcessLauncher(REPO_ROOT, binary_relative)
    return DockerComposeLauncher(REPO_ROOT, binary_relative)


@pytest.fixture(scope="session")
def docker_lock():
    """The shared Docker lock, held for the whole session when the Docker launcher is in use; None otherwise."""
    if launcher_mode() != "docker":
        yield None
        return
    lock_dir = Path(os.environ.get("ROTS_IT_DOCKER_LOCK_DIR", str(DEFAULT_LOCK_DIR)))
    wait_seconds = float(os.environ.get("ROTS_IT_LOCK_WAIT", DEFAULT_WAIT_SECONDS))
    lock = DockerLock(lock_dir, REPO_ROOT, wait_seconds=wait_seconds)
    lock.acquire(purpose="integration test session")
    try:
        yield lock
    finally:
        lock.release()


@pytest.fixture(scope="session")
def lib_provider(harness_launcher: ServerLauncher, docker_lock: DockerLock | None):
    """Builds each character set's template once per session; the templates are deleted at the end unless ROTS_IT_KEEP=1."""
    builder = LibTemplateBuilder(REPO_ROOT, INTEGRATION_ROOT / "world", harness_launcher)
    provider = RunLibProvider(builder, TEMPLATES_ROOT, session_seed())
    try:
        yield provider
    finally:
        if not keep_requested():
            provider.discard_templates()


def character_set_for(request: pytest.FixtureRequest) -> CharacterSet:
    """The characters the test's @pytest.mark.characters(...) declares; fails the test when it has none."""
    marker = request.node.get_closest_marker("characters")
    if marker is None or not marker.args:
        pytest.fail(f"{request.node.nodeid} uses the server fixture without @pytest.mark.characters(...)", pytrace=False)
    return CharacterSet(tuple(marker.args))


@pytest.fixture
def server(request: pytest.FixtureRequest, harness_launcher: ServerLauncher, lib_provider: RunLibProvider):
    """A fresh server on a lib holding the test's declared characters, as a HarnessServer.

    After the test it quits every session, stops the server, and fails the test when a session
    does not quit or the shutdown shows problems. The run directory is deleted unless the lib or server failed to set up, any test
    in the session has failed, this server's checks failed, or ROTS_IT_KEEP=1.
    """
    character_set = character_set_for(request)
    run_dir = RUNS_ROOT / uuid.uuid4().hex[:12]
    run_dir.mkdir(parents=True)
    try:
        lib_dir = lib_provider.lib_for(character_set, run_dir)
        handle = harness_launcher.start(run_dir, lib_dir, allocate_free_port(), session_seed())
    except Exception:
        # keep_run_directory never sees this case: the run directory is always kept, because
        # its game.log and lib are the only evidence of why the server did not start.
        print(f"\nthe test's lib or server could not be set up; run directory kept at {run_dir}")
        raise
    harness_server = HarnessServer(handle, lib_dir, run_dir, character_set, CrashMonitor(handle))
    teardown_problems: list[str] = []
    try:
        yield harness_server
    finally:
        try:
            teardown_problems = end_sessions(harness_server.sessions, handle.is_alive())
        finally:
            harness_launcher.stop(handle)
        shutdown_problems = harness_server.monitor.check_after_stop()
        if shutdown_problems:
            harness_server.crash_detected = True
        keep = keep_run_directory(keep_requested(), request.session.testsfailed, harness_server.crash_detected or bool(teardown_problems))
        if keep:
            print(f"\nrun directory kept at {run_dir}")
        else:
            shutil.rmtree(run_dir, ignore_errors=True)
        problems = teardown_problems + shutdown_problems
        if problems:
            pytest.fail("problems while ending the test:\n" + "\n".join(problems))


@pytest.fixture(autouse=True)
def fail_on_server_crash(request: pytest.FixtureRequest):
    """Fails a test that uses the server fixture when the server shows crash evidence by the end of the test."""
    # Requesting server here registers its finalizer first; finalizers run last in, first out,
    # so this check runs while the server is still up, before the server fixture quits sessions.
    harness_server = request.getfixturevalue("server") if "server" in request.fixturenames else None
    yield
    if harness_server is not None:
        problems = harness_server.monitor.check()
        if problems:
            harness_server.crash_detected = True
            pytest.fail("server problems during this test:\n" + "\n".join(problems))
