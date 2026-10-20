from __future__ import annotations

import ast
import configparser
from pathlib import Path

import pytest

from rots_harness.groups import GroupFileError, read_group_descriptions, registered_marker_names, unknown_marker_names

# tests/integration, whose groups/ folder and scenarios/ the consistency tests read.
INTEGRATION_ROOT = Path(__file__).resolve().parents[1]
# pytest's own markers a scenario may use; never a group.
PYTEST_BUILTIN_MARKERS = frozenset({"xfail", "skip", "skipif", "parametrize", "usefixtures", "filterwarnings"})
# pytest.ini markers lines: two plain markers and one taking arguments.
INI_LINES = [
    "scenario: boots its own server through the server fixture",
    "boot: the server boots on a harness lib and characters log in",
    "characters(*characters): the characters the test's server is built with",
]


def test_registered_names_are_read_from_the_ini_lines() -> None:
    assert registered_marker_names(INI_LINES) == ["scenario", "boot", "characters"]


def test_a_known_expression_has_no_unknown_names() -> None:
    assert unknown_marker_names("scenario and boot", ["scenario", "boot"]) == []


def test_a_typo_is_reported() -> None:
    assert unknown_marker_names("scenario and bot", ["scenario", "boot"]) == ["bot"]


def test_operators_and_parentheses_are_not_names() -> None:
    assert unknown_marker_names("(scenario or boot) and not boot", ["scenario", "boot"]) == []


def test_an_empty_expression_has_no_names() -> None:
    assert unknown_marker_names("", ["scenario"]) == []


def test_group_files_give_each_group_its_description_in_name_order(tmp_path: Path) -> None:
    (tmp_path / "summon.txt").write_text("summon cannot reach a dark room\n", encoding="utf-8")
    (tmp_path / "boot.txt").write_text("\n  the server boots  \n\n", encoding="utf-8")
    (tmp_path / "README.md").write_text("not a group\n", encoding="utf-8")
    (tmp_path / "nested.txt").mkdir()
    assert read_group_descriptions(tmp_path) == {"boot": "the server boots", "summon": "summon cannot reach a dark room"}


@pytest.mark.parametrize("file_name", ["bad-name.txt", "1st.txt", "or.txt"])
def test_a_group_file_not_named_like_a_marker_is_refused(tmp_path: Path, file_name: str) -> None:
    (tmp_path / file_name).write_text("a group\n", encoding="utf-8")
    with pytest.raises(GroupFileError, match="is not a marker name"):
        read_group_descriptions(tmp_path)


@pytest.mark.parametrize("text", ["", "\n\n", "first line\nsecond line\n"])
def test_a_group_file_without_exactly_one_line_is_refused(tmp_path: Path, text: str) -> None:
    (tmp_path / "boot.txt").write_text(text, encoding="utf-8")
    with pytest.raises(GroupFileError, match="on one line"):
        read_group_descriptions(tmp_path)


def pytest_ini_marker_names() -> set[str]:
    """The markers pytest.ini registers itself, none of them a group."""
    parser = configparser.ConfigParser(interpolation=None)
    parser.read(INTEGRATION_ROOT / "pytest.ini", encoding="utf-8")
    return set(registered_marker_names(parser.get("pytest", "markers").splitlines()))


def scenario_marker_names() -> set[str]:
    """Every name a scenario file uses as pytest.mark.<name>."""
    names = set()
    for path in (INTEGRATION_ROOT / "scenarios").glob("*.py"):
        for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
            is_mark = isinstance(node, ast.Attribute) and isinstance(node.value, ast.Attribute) and node.value.attr == "mark"
            if is_mark and isinstance(node.value.value, ast.Name) and node.value.value.id == "pytest":
                names.add(node.attr)
    return names


def test_the_group_folder_registers_exactly_the_groups_the_scenarios_mark() -> None:
    groups = set(read_group_descriptions(INTEGRATION_ROOT / "groups"))
    marked = scenario_marker_names() - pytest_ini_marker_names() - PYTEST_BUILTIN_MARKERS
    assert sorted(marked - groups) == [], "a scenario marks a group with no groups/<group>.txt"
    assert sorted(groups - marked) == [], "a group file names a group no scenario marks"


def test_no_group_file_names_a_marker_pytest_ini_registers() -> None:
    assert sorted(set(read_group_descriptions(INTEGRATION_ROOT / "groups")) & pytest_ini_marker_names()) == []
