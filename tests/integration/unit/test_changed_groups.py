from __future__ import annotations

import subprocess
from pathlib import Path
from typing import Optional

import pytest

from rots_harness import changed_groups, ci_groups

GROUPS = ["boot", "poison", "summon"]
# Each group file's description, by group name.
GROUP_DESCRIPTIONS = {
    "boot": "the server boots on a harness lib and characters log in",
    "poison": "poison ticks once per game hour for its duration, then wears off",
    "summon": "summon cannot reach a player in a dark room",
}
POISON_SCENARIO = "tests/integration/scenarios/test_poison.py"
POISON_SOURCE = "pytestmark = [pytest.mark.scenario, pytest.mark.poison]\n"
BASE = "origin/release-frodo-next"
COMMITTED_COMMAND = ["diff", "--name-only", f"{BASE}...HEAD"]
UNCOMMITTED_COMMAND = ["diff", "--name-only", "HEAD"]
UNTRACKED_COMMAND = ["ls-files", "--others", "--exclude-standard"]


def read_scenario(path: str) -> Optional[str]:
    return POISON_SOURCE if path == POISON_SCENARIO else None


def select(*changed: str) -> ci_groups.Selection:
    return changed_groups.select_local_groups(changed, read_scenario, GROUPS)


class FakeGit:
    """Answers the git commands changed_paths runs with canned output, recording each argument list."""

    def __init__(self, outputs: dict[tuple[str, ...], str]) -> None:
        # The standard output each git argument list prints.
        self._outputs = outputs
        # Every argument list run, in order.
        self.commands: list[list[str]] = []

    def __call__(self, arguments: list[str]) -> str:
        self.commands.append(arguments)
        return self._outputs[tuple(arguments)]


def test_changed_paths_joins_committed_uncommitted_and_untracked_files_without_repeats() -> None:
    fake_git = FakeGit({
        tuple(COMMITTED_COMMAND): "src/spells.cpp\nsrc/structs.h\n",
        tuple(UNCOMMITTED_COMMAND): "src/structs.h\nREADME.md\n",
        tuple(UNTRACKED_COMMAND): "tests/integration/scenarios/test_new.py\n\n",
    })
    assert changed_groups.changed_paths(BASE, fake_git) == [
        "src/spells.cpp", "src/structs.h", "README.md", "tests/integration/scenarios/test_new.py",
    ]
    assert fake_git.commands == [COMMITTED_COMMAND, UNCOMMITTED_COMMAND, UNTRACKED_COMMAND]


def test_no_changes_runs_boot_only() -> None:
    selection = select()
    assert selection.groups == ["boot"]
    assert selection.reasons == ["boot: always runs"]


def test_a_changed_scenario_adds_its_groups_as_in_ci() -> None:
    assert select(POISON_SCENARIO).groups == ["boot", "poison"]


def test_a_changed_harness_file_runs_every_group_as_in_ci() -> None:
    selection = select("tests/integration/rots_harness/session.py", "src/spells.cpp")
    assert selection.groups is None
    assert selection.reasons == ["every group: harness file tests/integration/rots_harness/session.py changed"]


def test_a_changed_game_header_runs_every_group() -> None:
    selection = select(POISON_SCENARIO, "src/structs.h")
    assert selection.groups is None
    assert selection.reasons == ["every group: header src/structs.h changed, and constants and layouts in headers reach any scenario"]


def test_a_changed_unit_test_header_selects_no_group() -> None:
    assert select("src/tests/game_tick_schedule_contract.h").groups == ["boot"]


def test_changed_game_code_is_listed_without_selecting_a_group() -> None:
    selection = select("src/spells.cpp", "src/tests/spells_tests.cpp", POISON_SCENARIO)
    assert selection.groups == ["boot", "poison"]
    assert selection.reasons == [
        "boot: always runs",
        "poison: test_poison.py changed",
        f"src/spells.cpp changed: {changed_groups.GAME_CODE_NOTE}",
    ]


def write_repo(repo_root: Path) -> None:
    """A repository with one file per group in GROUP_DESCRIPTIONS and the poison scenario."""
    groups_dir = repo_root / ci_groups.GROUPS_DIRECTORY
    groups_dir.mkdir(parents=True)
    for name, description in GROUP_DESCRIPTIONS.items():
        (groups_dir / f"{name}.txt").write_text(description + "\n", encoding="utf-8")
    scenario = repo_root / POISON_SCENARIO
    scenario.parent.mkdir(parents=True)
    scenario.write_text(POISON_SOURCE, encoding="utf-8")


class FakeRun:
    """Stands in for subprocess.run: answers git with canned output per changed path list and records every other command."""

    def __init__(self, committed: str, make_status: int = 0, git_status: int = 0) -> None:
        # What the committed-changes diff prints; the other two git lists print nothing.
        self._committed = committed
        # The exit status the make command returns.
        self._make_status = make_status
        # The exit status every git command returns.
        self._git_status = git_status
        # Every non-git command run, with its working directory.
        self.commands: list[tuple[list[str], Optional[Path]]] = []

    def __call__(self, arguments: list[str], **options: object) -> subprocess.CompletedProcess:
        if arguments[0] == "git":
            output = self._committed if arguments[3:] == COMMITTED_COMMAND else ""
            return subprocess.CompletedProcess(arguments, self._git_status, output, "fatal: bad revision")
        self.commands.append((arguments, options.get("cwd")))  # type: ignore[arg-type]
        return subprocess.CompletedProcess(arguments, self._make_status)


def run_main(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, fake_run: FakeRun, *extra: str) -> int:
    repo_root = tmp_path / "repo"
    write_repo(repo_root)
    monkeypatch.setattr(changed_groups.subprocess, "run", fake_run)
    return changed_groups.main(["--repo-root", str(repo_root), "--python", ".venv/bin/python", "--make", "gmake", *extra])


def test_main_runs_make_integration_with_the_chosen_groups(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]) -> None:
    fake_run = FakeRun(committed=f"{POISON_SCENARIO}\n", make_status=1)
    assert run_main(tmp_path, monkeypatch, fake_run) == 1
    assert fake_run.commands == [(["gmake", "integration", "PYTHON=.venv/bin/python", "GROUP=boot or poison"], tmp_path / "repo")]
    printed = capsys.readouterr().out
    assert "- poison: test_poison.py changed\n" in printed
    assert "GROUP=boot or poison" in printed


def test_main_runs_every_scenario_without_a_group(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]) -> None:
    fake_run = FakeRun(committed="src/structs.h\n")
    assert run_main(tmp_path, monkeypatch, fake_run) == 0
    assert fake_run.commands == [(["gmake", "integration", "PYTHON=.venv/bin/python"], tmp_path / "repo")]
    assert "every group" in capsys.readouterr().out


def test_main_list_only_prints_the_choice_and_runs_nothing(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]) -> None:
    fake_run = FakeRun(committed=f"{POISON_SCENARIO}\n")
    assert run_main(tmp_path, monkeypatch, fake_run, "--list-only") == 0
    assert fake_run.commands == []
    assert "GROUP=boot or poison" in capsys.readouterr().out


def test_main_fails_with_status_2_and_git_words_when_git_fails(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]) -> None:
    fake_run = FakeRun(committed="", git_status=128)
    assert run_main(tmp_path, monkeypatch, fake_run) == 2
    assert fake_run.commands == []
    assert "fatal: bad revision" in capsys.readouterr().err
