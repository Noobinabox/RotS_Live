"""Runs the scenario groups this checkout's changes reach, chosen by CI's rules plus a rule for game headers.

The changed paths are those committed since the merge base with --base, plus uncommitted changes
to tracked files and untracked files git does not ignore. `boot` always runs; changed scenario and
group files add their groups and a harness change runs every group, as in ci_groups (without a
pull request description). A changed game header (`src/**/*.h` outside `src/harness/` and
`src/tests/`) also runs every group. Changed game code (`.cpp`) selects no group; it is listed so
the reader can name groups with GROUP= or run every scenario.

Run from the repository root as `make integration-changed`; it prints the choice and then runs
`make integration` with it.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path
from typing import Callable, Iterable, Optional

from rots_harness import ci_groups
from rots_harness import groups as group_files

# The branch changes are measured from unless --base names another.
DEFAULT_BASE = "origin/release-frodo-next"
# The folder holding the game's sources.
GAME_SOURCE_DIRECTORY = "src/"
# Folders under src/ whose files are not game code: CI's rules cover the harness, and unit tests reach no scenario.
NON_GAME_SOURCE_PREFIXES = ("src/harness/", "src/tests/")
# Why a changed game source file is listed without adding a group.
GAME_CODE_NOTE = "game code selects no group yet; name groups with GROUP= or run every scenario with make integration"


class GitFailed(RuntimeError):
    """A git command exited unsuccessfully; the message carries the command and git's words."""


def changed_paths(base: str, run_git: Callable[[list[str]], str]) -> list[str]:
    """The paths changed since the merge base with base, uncommitted, or untracked, in that order without repeats.

    run_git runs git with the given arguments and returns its standard output; it raises GitFailed on failure.
    """
    listings = [
        run_git(["diff", "--name-only", f"{base}...HEAD"]),
        run_git(["diff", "--name-only", "HEAD"]),
        run_git(["ls-files", "--others", "--exclude-standard"]),
    ]
    paths: list[str] = []
    for listing in listings:
        for line in listing.splitlines():
            path = line.strip()
            if path and path not in paths:
                paths.append(path)
    return paths


def _is_game_source(path: str, suffix: str) -> bool:
    """Whether path is a game source file under src/ with the given suffix."""
    return path.startswith(GAME_SOURCE_DIRECTORY) and path.endswith(suffix) and not path.startswith(NON_GAME_SOURCE_PREFIXES)


def select_local_groups(changed: Iterable[str], read_scenario: Callable[[str], Optional[str]], groups: list[str]) -> ci_groups.Selection:
    """The groups a local run of these changed paths tests.

    read_scenario returns a changed scenario file's source, or None when the change deleted it.
    groups lists the registered groups.
    """
    changed_list = list(changed)
    selection = ci_groups.select_groups("pull_request", "", changed_list, read_scenario, groups)
    if selection.groups is None:
        return selection
    headers = [path for path in changed_list if _is_game_source(path, ".h")]
    if headers:
        return ci_groups.Selection(None, [f"every group: header {headers[0]} changed, and constants and layouts in headers reach any scenario"])
    game_code = [path for path in changed_list if _is_game_source(path, ".cpp")]
    selection.reasons.extend(f"{path} changed: {GAME_CODE_NOTE}" for path in game_code)
    return selection


def integration_command(make_program: str, python: str, selection: ci_groups.Selection) -> list[str]:
    """The make integration command that runs the selection: with GROUP for chosen groups, without it for every group."""
    command = [make_program, "integration", f"PYTHON={python}"]
    if selection.groups is not None:
        command.append(f"GROUP={selection.expression()}")
    return command


def main(argv: Optional[list[str]] = None) -> int:
    """Prints the reasons and the group expression, then runs make integration with it and returns its exit status.

    Returns 0 without running anything under --list-only, and 2 when git fails or a group file cannot be read.
    """
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--base", default=DEFAULT_BASE, help=f"the branch the changes are measured from (default {DEFAULT_BASE})")
    parser.add_argument("--repo-root", type=Path, default=Path("."), help="the repository root")
    parser.add_argument("--python", default=sys.executable, help="the Python make integration runs the scenarios with")
    parser.add_argument("--make", default="make", help="the make program that runs make integration")
    parser.add_argument("--list-only", action="store_true", help="print the choice without running it")
    arguments = parser.parse_args(argv)
    repo_root = arguments.repo_root

    def run_git(git_arguments: list[str]) -> str:
        completed = subprocess.run(["git", "-C", str(repo_root), *git_arguments], capture_output=True, text=True, check=False)
        if completed.returncode != 0:
            raise GitFailed(f"git {' '.join(git_arguments)} failed: {completed.stderr.strip()}")
        return completed.stdout

    def read_scenario(path: str) -> Optional[str]:
        scenario_path = repo_root / path
        return scenario_path.read_text(encoding="utf-8") if scenario_path.is_file() else None

    try:
        groups = list(group_files.read_group_descriptions(repo_root / ci_groups.GROUPS_DIRECTORY))
        changed = changed_paths(arguments.base, run_git)
    except (GitFailed, group_files.GroupFileError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    selection = select_local_groups(changed, read_scenario, groups)
    print(f"Changes since {arguments.base}, uncommitted changes and untracked files choose:")
    for reason in selection.reasons:
        print(f"- {reason}")
    if selection.groups is None:
        print("Choice: every group (no GROUP)")
    else:
        print(f"Choice: GROUP={selection.expression()}")
    if arguments.list_only:
        return 0
    # Flushed so the choice prints before make's output, which goes straight to the terminal.
    sys.stdout.flush()
    command = integration_command(arguments.make, arguments.python, selection)
    return subprocess.run(command, cwd=repo_root, check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
