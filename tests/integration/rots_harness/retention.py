"""Decides whether a scenario's run directory survives fixture teardown."""

from __future__ import annotations


def keep_run_directory(keep_requested: bool, tests_failed_so_far: int, this_server_failed: bool, interrupted: bool) -> bool:
    """True when the run directory must be kept for diagnosis.

    It is kept when the caller asked to keep run directories, when any test in the session has
    failed so far, when this server's own checks failed, or when the run was interrupted (Ctrl-C,
    SIGTERM or SIGHUP), which pytest never counts as a failure. tests_failed_so_far is pytest's
    session counter, which does not yet include a failure raised during teardown (such as the
    crash check), because pytest reports it only after every finalizer has run; the caller
    passes that case as this_server_failed.
    """
    return keep_requested or tests_failed_so_far > 0 or this_server_failed or interrupted
