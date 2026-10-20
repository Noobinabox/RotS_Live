"""The scenario groups, one file each under tests/integration/groups, and the check that a pytest -m
expression names only registered markers, so a misspelt group stops the run."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Iterable

# A marker name as pytest's -m grammar reads an identifier.
MARKER_NAME_PATTERN = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
# Words of the -m grammar that are operators, not marker names.
EXPRESSION_KEYWORDS = frozenset({"and", "or", "not"})
# The folder under tests/integration holding one <group>.txt file per scenario group.
GROUPS_DIRECTORY_NAME = "groups"
# The suffix of a group file; the file's name without it is the group's marker name.
GROUP_FILE_SUFFIX = ".txt"


class GroupFileError(ValueError):
    """A group file's name is not a marker name, or the file does not hold exactly one non-empty line."""


def registered_marker_names(ini_lines: Iterable[str]) -> list[str]:
    """Marker names from pytest.ini's markers lines ("name: help" or "name(args): help")."""
    names = []
    for line in ini_lines:
        match = MARKER_NAME_PATTERN.match(line.strip())
        if match:
            names.append(match.group(0))
    return names


def unknown_marker_names(expression: str, registered: Iterable[str]) -> list[str]:
    """Names in a -m expression that no marker registers, in the order they appear."""
    known = set(registered)
    unknown = []
    for name in MARKER_NAME_PATTERN.findall(expression):
        if name not in EXPRESSION_KEYWORDS and name not in known and name not in unknown:
            unknown.append(name)
    return unknown


def read_group_descriptions(groups_dir: Path) -> dict[str, str]:
    """Each group's name and one-line description from groups_dir/<name>.txt, in name order.

    Files with another suffix are ignored. Raises GroupFileError for a file whose name is not a
    marker name or that does not hold exactly one non-empty line, and OSError when the folder
    cannot be read.
    """
    descriptions = {}
    for path in sorted(groups_dir.iterdir()):
        if not path.is_file() or path.suffix != GROUP_FILE_SUFFIX:
            continue
        name = path.stem
        if MARKER_NAME_PATTERN.fullmatch(name) is None or name in EXPRESSION_KEYWORDS:
            raise GroupFileError(f"{path}: {name!r} is not a marker name (letters, digits and underscores, not starting with a digit)")
        lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
        if len(lines) != 1:
            raise GroupFileError(f"{path}: a group file holds its description on one line; found {len(lines)} non-empty lines")
        descriptions[name] = lines[0]
    return descriptions
