"""This pytest session's place in the lock directory shared by every Docker job on the host."""

from __future__ import annotations

import os
import sys
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Optional

DEFAULT_LOCK_DIR = Path("/tmp/rots-docker-lock")
LOCK_FILE_PREFIX = "harness-it"
DEFAULT_WAIT_SECONDS = 600.0
POLL_SECONDS = 5.0


class DockerLockHeld(RuntimeError):
    """Another session's lock file is in the lock directory; the message names the holders."""


def _report_to_stderr(message: str) -> None:
    print(message, file=sys.stderr, flush=True)


class DockerLock:
    """One uniquely named lock file per pytest session, held for the whole session.

    Unique names let two harness sessions on one host see each other, and holding the file for
    the session keeps another Docker job from running between two tests.
    """

    def __init__(
        self,
        lock_dir: Path | None,
        repo_root: Path,
        wait_seconds: float = DEFAULT_WAIT_SECONDS,
        poll_seconds: float = POLL_SECONDS,
        clock: Callable[[], float] = time.monotonic,
        sleep: Callable[[float], None] = time.sleep,
        report: Callable[[str], None] = _report_to_stderr,
    ) -> None:
        """Prepares the lock without touching the directory; a lock_dir of None disables locking."""
        # The shared lock directory, or None when this session runs nothing in Docker.
        self._lock_dir = lock_dir
        # The checkout this session runs from, recorded in the lock file for whoever finds it.
        self._repo_root = repo_root
        # How long acquire keeps retrying while another session holds the lock.
        self._wait_seconds = wait_seconds
        # The pause between two attempts while waiting.
        self._poll_seconds = poll_seconds
        # Monotonic seconds that measure the wait; replaced in tests.
        self._clock = clock
        # Pauses between attempts; replaced in tests.
        self._sleep = sleep
        # Tells the person running the tests who holds the lock while acquire waits.
        self._report = report
        # This session's own lock file while it is held, otherwise None.
        self._lock_path: Optional[Path] = None

    @property
    def path(self) -> Path | None:
        """This session's lock file while held, otherwise None."""
        return self._lock_path

    def acquire(self, purpose: str) -> None:
        """Takes the lock, waiting up to wait_seconds while another session holds it.

        Raises DockerLockHeld when the wait runs out, and RuntimeError at once when the lock
        directory is missing.
        """
        if self._lock_dir is None:
            return
        deadline = self._clock() + self._wait_seconds
        last_reported: Optional[str] = None
        while True:
            try:
                self._try_acquire(purpose)
                return
            except DockerLockHeld as held:
                if self._clock() >= deadline:
                    raise
                holders = str(held)
                if holders != last_reported:
                    self._report(f"waiting up to {self._wait_seconds:.0f}s for the Docker lock: {holders}")
                    last_reported = holders
                self._sleep(self._poll_seconds)

    def _try_acquire(self, purpose: str) -> None:
        """Makes one attempt to take the lock; raises DockerLockHeld when another session holds it."""
        assert self._lock_dir is not None
        if not self._lock_dir.is_dir():
            raise RuntimeError(f"Docker lock directory {self._lock_dir} is missing; create it (see tests/integration/README.md, Shared Docker lock)")
        others = sorted(path.name for path in self._lock_dir.glob("*.lock"))
        if others:
            raise DockerLockHeld(f"another session holds the Docker lock ({', '.join(others)}); see tests/integration/README.md")
        candidate = self._lock_dir / f"{LOCK_FILE_PREFIX}-{os.getpid()}-{uuid.uuid4().hex[:8]}.lock"
        started = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        body = (
            f"session: {LOCK_FILE_PREFIX}\n"
            f"pid: {os.getpid()}\n"
            f"repo: {self._repo_root}\n"
            "service: rots\n"
            f"purpose: {purpose}\n"
            f"start: {started}\n"
            "duration: pytest session\n"
        )
        try:
            # Exclusive creation never overwrites another session's file.
            with candidate.open("x", encoding="utf-8") as handle:
                handle.write(body)
        except FileExistsError as collision:
            raise DockerLockHeld(f"lock name collision on {candidate.name}") from collision
        self._lock_path = candidate
        # Two sessions starting together can both find the directory empty. Rechecking after
        # this file exists means at least one of them sees the other and backs off, so both may
        # retry but they never both run.
        others = sorted(path.name for path in self._lock_dir.glob("*.lock") if path != candidate)
        if others:
            self.release()
            raise DockerLockHeld(f"another session took the Docker lock at the same time ({', '.join(others)}); see tests/integration/README.md")

    def release(self) -> None:
        """Removes this session's lock file if it holds one; does nothing otherwise."""
        if self._lock_path is not None:
            try:
                self._lock_path.unlink()
            except FileNotFoundError:
                pass
        self._lock_path = None
