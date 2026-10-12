"""Reports crashes and unexpected SYSERR lines from a server's log and process state."""

from __future__ import annotations

from typing import Callable, Optional

from rots_harness.launcher import ServerHandle

# SYSERR fragments a harness boot is known to print. A boot on the integration branch prints none (checked 2026-10-11); add an entry only with the reason it is harmless.
DEFAULT_ALLOWED: tuple[str, ...] = ()
# Lines that sanitizer runtimes write when they find an error.
SANITIZER_MARKERS = ("AddressSanitizer", "LeakSanitizer", "UndefinedBehaviorSanitizer", "runtime error:")
# sigsegv_handler() in comm.cpp prints this ahead of its backtrace.
SIGNAL_MARKER = "Error: signal"
# hupsig() in signals.cpp logs this on every requested stop; it is not a crash.
SHUTDOWN_SIGNAL_LINE = "Received SIGHUP, SIGINT, or SIGTERM"


class CrashMonitor:
    """Watches one server for crash evidence, reporting each log line and exit at most once."""

    def __init__(self, handle: ServerHandle, allowed_syserr_fragments: tuple[str, ...] = DEFAULT_ALLOWED) -> None:
        # The server whose log and process are inspected.
        self._handle = handle
        # A SYSERR line containing any of these fragments is not reported.
        self._allowed = allowed_syserr_fragments
        # Bytes of the log already inspected; each check reads only what follows.
        self._log_offset = 0
        # Whether check() has reported the process's exit, so check_after_stop() does not repeat it.
        self._exit_reported = False

    def check(self) -> list[str]:
        """Problems since the last check: crash markers, unexpected SYSERRs, and a process exit."""
        problems = self._log_problems()
        status = self._handle.exit_status()
        if status is not None and not self._exit_reported:
            self._exit_reported = True
            problems.append(f"server process exited with status {status}")
        return problems

    def check_after_stop(self) -> list[str]:
        """Problems once the launcher has stopped the server.

        Reports crash markers and unexpected SYSERRs written since the last check (the shutdown
        notice is expected), and any exit status other than 0, including a process that has not
        exited. An exit check() already reported is not reported again.
        """
        problems = self._log_problems(ignore_line=lambda line: SHUTDOWN_SIGNAL_LINE in line)
        if self._exit_reported:
            return problems
        status = self._handle.exit_status()
        # hupsig() ends in exit(0), so a signal death or a sanitizer abort during shutdown
        # shows as a non-zero status.
        if status is None:
            problems.append("server process still running after stop")
        elif status != 0:
            problems.append(f"server process exited with status {status} after stop")
        return problems

    def _log_problems(self, ignore_line: Optional[Callable[[str], bool]] = None) -> list[str]:
        """Problems in the log lines written since the last read, skipping lines ignore_line accepts."""
        problems: list[str] = []
        for line in self._new_lines():
            if ignore_line is not None and ignore_line(line):
                continue
            if any(marker in line for marker in SANITIZER_MARKERS) or SIGNAL_MARKER in line:
                problems.append(f"crash marker in log: {line.strip()}")
            elif "SYSERR" in line and not any(fragment in line for fragment in self._allowed):
                problems.append(f"unexpected SYSERR: {line.strip()}")
        return problems

    def _new_lines(self) -> list[str]:
        """The log lines written since the last read; none while the log does not exist."""
        if not self._handle.log_path.exists():
            return []
        data = self._handle.log_path.read_bytes()
        fresh = data[self._log_offset:]
        self._log_offset = len(data)
        return fresh.decode("latin-1", errors="replace").splitlines()
