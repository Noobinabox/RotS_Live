"""Chooses the scenario groups a CI run tests, from the pull request's description and changed files.

`boot` always runs. A description token `@integration-test-<group>` adds that group, and
`@integration-test-all` runs every group. A changed scenario file adds the groups it marks, and a
changed group file (tests/integration/groups/<group>.txt) adds its group. These run every group:
a push; a change to the harness itself; a change to `src/CMakeLists.txt`, whose compile settings
reach every program; and a change to the `lib/` files the harness copies into every test's lib
(`lib/text/` and the `lib/misc` files in libbuilder.TRACKED_MISC_FILES). Only a run of every group
is split across parallel shards; a run of chosen groups is one job.

Run from tests/integration as `python3 -m rots_harness.ci_groups`; the description arrives in
the PR_BODY environment variable so it is never part of a shell command. --changed-files names a
file of NUL-separated paths, as `git diff --name-only -z --no-renames` prints them.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterable, Optional, TextIO

from rots_harness import groups as group_files
from rots_harness import libbuilder

ALWAYS_RUN_GROUP = "boot"
ALL_GROUPS_TOKEN = "all"
# A token in the description; the name stops at the first character a marker name cannot hold.
DESCRIPTION_TOKEN = re.compile(r"(?<![\w-])@integration-test-([A-Za-z0-9_]+)")
# HTML comments are dropped first, so a pull request template can show the syntax.
HTML_COMMENT = re.compile(r"<!--.*?-->", re.S)
SCENARIO_MARKER = re.compile(r"pytest\.mark\.([A-Za-z_][A-Za-z0-9_]*)")
SCENARIO_DIRECTORY = "tests/integration/scenarios/"
# One <group>.txt per group; a change to one adds only its group, so it is not a harness path.
GROUPS_DIRECTORY = f"tests/integration/{group_files.GROUPS_DIRECTORY_NAME}/"
# Paths whose change can affect any scenario, so every group runs.
HARNESS_PATH_PREFIXES = (
    "tests/integration/rots_harness/",
    "tests/integration/world/",
    "src/harness/",
    "src/tools/rotstool/",
)
# The per-zone fixture notes under the world: documentation the harness never copies into a lib.
WORLD_NOTES_DIRECTORY = "tests/integration/world/notes/"
HARNESS_FILES = frozenset({
    "tests/integration/conftest.py",
    "tests/integration/pytest.ini",
    "tests/integration/requirements.txt",
    "Dockerfile",
    ".github/workflows/integration.yml",
    ".github/actions/dev-image/action.yml",
})
# The build file whose compile settings reach every program the scenarios run.
BUILD_FILE = "src/CMakeLists.txt"
# The lib folder the lib builder copies whole into every test's lib.
COPIED_LIB_TEXT_DIRECTORY = "lib/text/"
# The lib/misc files the lib builder copies into every test's lib.
COPIED_LIB_MISC_FILES = frozenset(f"lib/misc/{name}" for name in libbuilder.TRACKED_MISC_FILES)
# Separates the paths in a --changed-files file, as git's -z option writes them.
CHANGED_PATH_SEPARATOR = "\0"
# How text naming a changed path is written: a byte that is not UTF-8 arrives as a surrogate,
# which strict encoding refuses, so it is written as a backslash escape instead.
REASON_ENCODING_ERRORS = "backslashreplace"


class UnknownGroups(ValueError):
    """The description names groups no group file registers; the message lists the valid ones."""


@dataclass
class Selection:
    """The groups to run, None meaning every group, and one line per reason."""

    # The groups to run in order, or None to run every scenario.
    groups: Optional[list[str]]
    # Why each group or the whole suite was chosen, for the job summary.
    reasons: list[str] = field(default_factory=list)

    def expression(self) -> str:
        """The GROUP value for make integration; empty runs every scenario."""
        return "" if self.groups is None else " or ".join(self.groups)

    def shard_numbers(self, full_suite_shards: int) -> list[int]:
        """The CI shards to run: 1 to full_suite_shards for every group, and only shard 1 otherwise,
        since a few groups do not repay building the server once per shard."""
        return list(range(1, full_suite_shards + 1)) if self.groups is None else [1]


def changed_group_file(path: str) -> Optional[str]:
    """The group a changed path's group file names, or None when the path is not a group file."""
    if not (path.startswith(GROUPS_DIRECTORY) and path.endswith(group_files.GROUP_FILE_SUFFIX)):
        return None
    relative = path[len(GROUPS_DIRECTORY):]
    return None if "/" in relative else relative[: -len(group_files.GROUP_FILE_SUFFIX)]


def description_tokens(body: str) -> list[str]:
    """The group names the description's tokens give, in order and without repeats, outside HTML comments."""
    names = []
    for name in DESCRIPTION_TOKEN.findall(HTML_COMMENT.sub("", body)):
        if name not in names:
            names.append(name)
    return names


def scenario_groups(source: str, groups: Iterable[str]) -> list[str]:
    """The groups a scenario file's source marks."""
    known = set(groups)
    found = []
    for name in SCENARIO_MARKER.findall(source):
        if name in known and name not in found:
            found.append(name)
    return found


def is_harness_path(path: str) -> bool:
    """Whether a change to this repository path can affect every scenario."""
    if path.startswith(WORLD_NOTES_DIRECTORY):
        return False
    if path in HARNESS_FILES or path.startswith(HARNESS_PATH_PREFIXES):
        return True
    # A scenario helper or conftest is shared by every scenario file.
    return path.startswith(SCENARIO_DIRECTORY) and not Path(path).name.startswith("test_")


def every_group_reason(path: str) -> Optional[str]:
    """Why a change to this repository path runs every group, or None when it does not."""
    if is_harness_path(path):
        return f"every group: harness file {path} changed"
    if path == BUILD_FILE:
        return f"every group: {path} changed, and its compile settings reach every program"
    if path.startswith(COPIED_LIB_TEXT_DIRECTORY) or path in COPIED_LIB_MISC_FILES:
        return f"every group: {path} changed, and the harness copies it into every test's lib"
    return None


def read_changed_paths(listing: str) -> list[str]:
    """The paths in a NUL-separated listing, each kept whole; an empty listing holds none."""
    return [path for path in listing.split(CHANGED_PATH_SEPARATOR) if path]


def escape_unencodable_output(stream: TextIO) -> None:
    """Makes stream write a character its encoding cannot hold as a backslash escape instead of raising.

    A stream without reconfigure, such as one a caller substituted, is left as it is.
    """
    reconfigure = getattr(stream, "reconfigure", None)
    if reconfigure is not None:
        reconfigure(errors=REASON_ENCODING_ERRORS)


def select_groups(
    event: str, body: str, changed_paths: Iterable[str], read_scenario: Callable[[str], Optional[str]], groups: list[str],
) -> Selection:
    """The groups a CI run tests.

    read_scenario returns a changed scenario file's source, or None when the change deleted it.
    groups lists the registered groups; a changed group file adds its group only while it is one.
    Raises UnknownGroups when the description names a group no group file registers.
    """
    if event == "push":
        return Selection(None, ["every group: a push runs the whole suite"])
    tokens = description_tokens(body)
    unknown = [name for name in tokens if name != ALL_GROUPS_TOKEN and name not in groups]
    if unknown:
        raise UnknownGroups(
            f"the description names unknown integration groups: {', '.join(unknown)}; "
            f"registered groups: {', '.join(groups)} (or {ALL_GROUPS_TOKEN})"
        )
    if ALL_GROUPS_TOKEN in tokens:
        return Selection(None, [f"every group: the description has @integration-test-{ALL_GROUPS_TOKEN}"])
    changed = list(changed_paths)
    for path in changed:
        reason = every_group_reason(path)
        if reason is not None:
            return Selection(None, [reason])
    selected = [ALWAYS_RUN_GROUP]
    reasons = [f"{ALWAYS_RUN_GROUP}: always runs"]
    for name in tokens:
        if name not in selected:
            selected.append(name)
            reasons.append(f"{name}: named in the description")
    for path in changed:
        group_name = changed_group_file(path)
        if group_name is not None:
            if group_name in groups and group_name not in selected:
                selected.append(group_name)
                reasons.append(f"{group_name}: its group file {Path(path).name} changed")
            continue
        if not (path.startswith(SCENARIO_DIRECTORY) and path.endswith(".py")):
            continue
        source = read_scenario(path)
        if source is None:
            continue
        for name in scenario_groups(source, groups):
            if name not in selected:
                selected.append(name)
                reasons.append(f"{name}: {Path(path).name} changed")
    return Selection(selected, reasons)


def summary_markdown(selection: Selection, shard_count: int = 1) -> str:
    """The job summary: what runs, why, and across how many shards."""
    ran = "every group" if selection.groups is None else ", ".join(f"`{name}`" for name in selection.groups)
    lines = [f"### Integration groups: {ran}", ""]
    lines.extend(f"- {reason}" for reason in selection.reasons)
    if shard_count > 1:
        lines.extend(["", f"Split across {shard_count} parallel shards."])
    return "\n".join(lines) + "\n"


def main(argv: Optional[list[str]] = None) -> int:
    """Writes `group=<expression>` and `shards=<JSON list>` to --output and the summary to --summary.

    Returns 2 when the description names an unknown group or a group file cannot be read.
    """
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--event", required=True, help="the GitHub event name: pull_request or push")
    parser.add_argument("--changed-files", type=Path, help="a file of NUL-separated changed paths, as git diff --name-only -z prints them")
    parser.add_argument("--repo-root", type=Path, default=Path("../.."), help="the repository root")
    parser.add_argument("--output", type=Path, required=True, help="the file that receives group=<expression>")
    parser.add_argument("--summary", type=Path, help="the file that receives the markdown summary")
    parser.add_argument("--full-suite-shards", type=int, default=1, help="how many shards a run of every group uses")
    arguments = parser.parse_args(argv)
    if arguments.full_suite_shards < 1:
        parser.error("--full-suite-shards must be at least 1")
    repo_root = arguments.repo_root
    try:
        groups = list(group_files.read_group_descriptions(repo_root / GROUPS_DIRECTORY))
    except (group_files.GroupFileError, OSError) as error:
        print(f"error: cannot read the scenario groups: {error}", file=sys.stderr)
        return 2
    changed = []
    if arguments.changed_files is not None:
        # surrogateescape keeps the bytes of a file name that is not valid UTF-8, so the name still
        # reaches read_scenario as listed; the reasons that quote it are written escaped below.
        changed = read_changed_paths(arguments.changed_files.read_text(encoding="utf-8", errors="surrogateescape"))

    def read_scenario(path: str) -> Optional[str]:
        scenario_path = repo_root / path
        return scenario_path.read_text(encoding="utf-8") if scenario_path.is_file() else None

    try:
        selection = select_groups(arguments.event, os.environ.get("PR_BODY", ""), changed, read_scenario, groups)
    except UnknownGroups as unknown:
        print(f"error: {unknown}", file=sys.stderr)
        return 2
    shards = selection.shard_numbers(arguments.full_suite_shards)
    with arguments.output.open("a", encoding="utf-8") as output:
        output.write(f"group={selection.expression()}\n")
        output.write(f"shards={json.dumps(shards)}\n")
    summary = summary_markdown(selection, len(shards))
    if arguments.summary is not None:
        with arguments.summary.open("a", encoding="utf-8", errors=REASON_ENCODING_ERRORS) as summary_file:
            summary_file.write(summary)
    escape_unencodable_output(sys.stdout)
    print(summary, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
