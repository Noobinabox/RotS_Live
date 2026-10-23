from __future__ import annotations

import ast
import configparser
from pathlib import Path

import pytest

from rots_harness.groups import (
    Group,
    GroupFileError,
    read_group_descriptions,
    read_groups,
    registered_marker_names,
    subgroup_marking_problems,
    subgroups_by_parent,
    unknown_marker_names,
)

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
    assert read_group_descriptions(tmp_path) == {"boot": "the server boots", "summon": "summon cannot reach a dark room"}


def write_group_tree(groups_dir: Path) -> None:
    """Two top-level groups, shaping with two subgroups, and files the reader ignores."""
    (groups_dir / "shaping.txt").write_text("the shaping editors\n", encoding="utf-8")
    (groups_dir / "boot.txt").write_text("the server boots\n", encoding="utf-8")
    (groups_dir / "shaping").mkdir()
    (groups_dir / "shaping" / "zone_editor.txt").write_text("the zone editor\n", encoding="utf-8")
    (groups_dir / "shaping" / "commands.txt").write_text("the disabled commands\n", encoding="utf-8")
    (groups_dir / "shaping" / "README.md").write_text("not a subgroup\n", encoding="utf-8")


def test_a_subfolder_registers_its_files_as_subgroups_after_their_group(tmp_path: Path) -> None:
    write_group_tree(tmp_path)
    (tmp_path / "summon.txt").write_text("summon\n", encoding="utf-8")
    assert read_groups(tmp_path) == [
        Group("boot", "the server boots"),
        Group("shaping", "the shaping editors"),
        Group("shaping_commands", "the disabled commands", parent="shaping"),
        Group("shaping_zone_editor", "the zone editor", parent="shaping"),
        Group("summon", "summon"),
    ]


def test_group_descriptions_include_the_subgroups_in_registration_order(tmp_path: Path) -> None:
    write_group_tree(tmp_path)
    assert list(read_group_descriptions(tmp_path).items()) == [
        ("boot", "the server boots"),
        ("shaping", "the shaping editors"),
        ("shaping_commands", "the disabled commands"),
        ("shaping_zone_editor", "the zone editor"),
    ]


def test_subgroups_by_parent_lists_only_the_groups_with_subgroups(tmp_path: Path) -> None:
    write_group_tree(tmp_path)
    assert subgroups_by_parent(read_groups(tmp_path)) == {"shaping": ["shaping_commands", "shaping_zone_editor"]}


def test_a_subfolder_without_its_group_file_is_refused(tmp_path: Path) -> None:
    (tmp_path / "boot.txt").write_text("the server boots\n", encoding="utf-8")
    (tmp_path / "shaping").mkdir()
    (tmp_path / "shaping" / "zone_editor.txt").write_text("the zone editor\n", encoding="utf-8")
    with pytest.raises(GroupFileError, match="no group file shaping.txt beside it"):
        read_groups(tmp_path)


def test_a_folder_named_like_a_group_file_is_refused(tmp_path: Path) -> None:
    (tmp_path / "nested.txt").mkdir()
    with pytest.raises(GroupFileError, match="no group file nested.txt.txt beside it"):
        read_groups(tmp_path)


def test_a_folder_inside_a_subgroup_folder_is_refused(tmp_path: Path) -> None:
    write_group_tree(tmp_path)
    (tmp_path / "shaping" / "zone_editor").mkdir()
    with pytest.raises(GroupFileError, match="a subgroup has no subgroups"):
        read_groups(tmp_path)


@pytest.mark.parametrize("file_name", ["zone-editor.txt", "1st.txt", "or.txt"])
def test_a_subgroup_file_not_named_like_a_marker_is_refused(tmp_path: Path, file_name: str) -> None:
    write_group_tree(tmp_path)
    (tmp_path / "shaping" / file_name).write_text("a subgroup\n", encoding="utf-8")
    with pytest.raises(GroupFileError, match="is not a marker name"):
        read_groups(tmp_path)


def test_a_subgroup_file_without_exactly_one_line_is_refused(tmp_path: Path) -> None:
    write_group_tree(tmp_path)
    (tmp_path / "shaping" / "saving.txt").write_text("first line\nsecond line\n", encoding="utf-8")
    with pytest.raises(GroupFileError, match="on one line"):
        read_groups(tmp_path)


def test_a_subgroup_named_like_a_top_level_group_is_refused(tmp_path: Path) -> None:
    write_group_tree(tmp_path)
    (tmp_path / "shaping_zone_editor.txt").write_text("the zone editor again\n", encoding="utf-8")
    with pytest.raises(GroupFileError, match=r"shaping_zone_editor.txt registers shaping_zone_editor, which .*zone_editor.txt also registers"):
        read_groups(tmp_path)


def test_two_subgroups_registering_one_name_are_refused(tmp_path: Path) -> None:
    # a/b_c.txt and a_b/c.txt would both register a_b_c.
    for name in ("a", "a_b"):
        (tmp_path / f"{name}.txt").write_text(f"group {name}\n", encoding="utf-8")
        (tmp_path / name).mkdir()
    (tmp_path / "a" / "b_c.txt").write_text("one\n", encoding="utf-8")
    (tmp_path / "a_b" / "c.txt").write_text("two\n", encoding="utf-8")
    with pytest.raises(GroupFileError, match=r"c.txt registers a_b_c, which .*b_c.txt also registers"):
        read_groups(tmp_path)


# shaping's subgroups, for the marking checks.
SUBGROUPS = {"shaping": ["shaping_commands", "shaping_zone_editor"]}


def test_a_test_with_its_group_and_one_subgroup_breaks_no_rule() -> None:
    tests = [
        ("test_a.py::test_one", {"scenario", "shaping", "shaping_commands"}),
        ("test_b.py::test_two", {"scenario", "boot"}),
    ]
    assert subgroup_marking_problems(tests, SUBGROUPS) == []


def test_a_test_of_a_group_with_subgroups_needs_one_of_them() -> None:
    problems = subgroup_marking_problems([("test_a.py::test_one", {"scenario", "shaping"})], SUBGROUPS)
    assert problems == ["test_a.py::test_one: marks shaping but none of its subgroups (shaping_commands, shaping_zone_editor)"]


def test_a_test_marking_two_subgroups_of_one_group_is_reported() -> None:
    problems = subgroup_marking_problems([("test_a.py::test_one", {"shaping", "shaping_zone_editor", "shaping_commands"})], SUBGROUPS)
    assert problems == ["test_a.py::test_one: marks 2 subgroups of shaping (shaping_commands, shaping_zone_editor); a test marks exactly one"]


def test_a_subgroup_without_its_group_is_reported() -> None:
    problems = subgroup_marking_problems([("test_a.py::test_one", {"scenario", "shaping_commands"})], SUBGROUPS)
    assert problems == ["test_a.py::test_one: marks the subgroup shaping_commands without its group shaping"]


def test_every_problem_of_every_test_is_reported_in_order() -> None:
    tests = [
        ("test_a.py::test_one", {"shaping"}),
        ("test_a.py::test_two", {"shaping", "shaping_commands"}),
        ("test_a.py::test_three", {"shaping_commands", "shaping_zone_editor"}),
    ]
    assert subgroup_marking_problems(tests, SUBGROUPS) == [
        "test_a.py::test_one: marks shaping but none of its subgroups (shaping_commands, shaping_zone_editor)",
        "test_a.py::test_three: marks 2 subgroups of shaping (shaping_commands, shaping_zone_editor); a test marks exactly one",
        "test_a.py::test_three: marks the subgroup shaping_commands without its group shaping",
        "test_a.py::test_three: marks the subgroup shaping_zone_editor without its group shaping",
    ]


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
