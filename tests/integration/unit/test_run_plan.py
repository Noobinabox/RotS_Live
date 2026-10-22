from __future__ import annotations

import threading
from pathlib import Path
from typing import Optional

import pytest

from rots_harness import run_plan
from rots_harness.collect_report import CollectedTest
from rots_harness.run_plan import AnswerOutcome, Gate

# The registered groups, in registration order; shaping marks no selected test.
GROUPS = ["boot", "poison", "shaping", "summon"]
# Four selected tests; the second is in two groups.
TESTS = [
    CollectedTest("scenarios/test_boot.py::test_one", frozenset({"scenario", "boot"})),
    CollectedTest("scenarios/test_boot.py::test_two", frozenset({"scenario", "boot", "poison", "characters"})),
    CollectedTest("scenarios/test_poison.py::test_three", frozenset({"scenario", "poison"})),
    CollectedTest("scenarios/test_summon.py::test_four", frozenset({"scenario", "summon"})),
]
# Every selected test's node id, in collection order.
ALL_IDS = [test.node_id for test in TESTS]
# Recorded seconds for the first two tests; the others count the default.
DURATIONS = {ALL_IDS[0]: 30.0, ALL_IDS[1]: 90.0, "scenarios/test_gone.py::test_old": 600.0}
# The pytest command make integration ran before the plan, for GROUP=boot and JOBS=8.
TODAYS_BOOT_COMMAND = [
    "py", "-m", "pytest", "tests/integration/scenarios", "-q", "-m", "scenario and (boot)", "-n", "8",
    "--junitxml=build/integration/results.xml", "--durations=25",
]


def plan(skipped: Optional[list[str]] = None, jobs: int = 1) -> run_plan.RunPlan:
    return run_plan.build_plan(TESTS, GROUPS, skipped or [], DURATIONS, jobs)


# Plan building


def test_each_group_counts_every_test_it_marks_and_empty_groups_are_dropped() -> None:
    built = plan()
    assert [(group.name, len(group.test_ids)) for group in built.offered_groups] == [("boot", 2), ("poison", 2), ("summon", 1)]
    assert built.skipped_groups == []


def test_the_total_counts_a_test_in_two_groups_once() -> None:
    assert plan().kept_test_ids([]) == ALL_IDS


def test_skipped_groups_are_listed_as_skipped_and_not_offered() -> None:
    built = plan(skipped=["summon", "shaping"])
    assert [group.name for group in built.offered_groups] == ["boot", "poison"]
    assert [group.name for group in built.skipped_groups] == ["summon"]
    assert built.kept_test_ids([]) == ALL_IDS[:3]


def test_unticking_a_group_drops_every_test_it_marks_even_one_another_kept_group_marks() -> None:
    assert plan().kept_test_ids(["poison"]) == [ALL_IDS[0], ALL_IDS[3]]


def test_a_test_in_no_group_still_counts_in_the_total() -> None:
    ungrouped = CollectedTest("scenarios/test_x.py::test_loose", frozenset({"scenario"}))
    built = run_plan.build_plan([*TESTS, ungrouped], GROUPS, [], {}, 1)
    assert built.kept_test_ids(["boot", "poison", "summon"]) == [ungrouped.node_id]
    assert built.ungrouped_count == 1


def test_skip_groups_are_split_on_spaces_and_commas_without_repeats() -> None:
    assert run_plan.parse_skip_groups(" poison, summon  poison,", GROUPS) == ["poison", "summon"]
    assert run_plan.parse_skip_groups("", GROUPS) == []


def test_an_unregistered_skip_group_is_rejected_with_the_valid_names() -> None:
    with pytest.raises(ValueError, match="bot.*boot, poison, shaping, summon"):
        run_plan.parse_skip_groups("poison bot", GROUPS)


# Estimates


def test_the_estimate_uses_recorded_times_and_the_default_for_the_rest() -> None:
    # 30 + 90 + 15 + 15 seconds.
    assert plan().estimated_minutes(ALL_IDS) == pytest.approx(2.5)
    assert plan().untimed_count(ALL_IDS) == 2


def test_the_estimate_is_divided_by_the_workers() -> None:
    assert plan(jobs=2).estimated_minutes(ALL_IDS) == pytest.approx(1.25)
    assert plan(jobs=0).estimated_minutes(ALL_IDS) == pytest.approx(2.5)


def test_a_group_estimate_covers_its_own_tests() -> None:
    poison = plan().offered_groups[1]
    assert plan().estimated_minutes(poison.test_ids) == pytest.approx(105 / 60)


@pytest.mark.parametrize("minutes, text", [(0.0, "0.0 min"), (0.44, "0.4 min"), (9.94, "9.9 min"), (12.6, "13 min")])
def test_minutes_show_one_decimal_below_ten_and_whole_minutes_above(minutes: float, text: str) -> None:
    assert run_plan.format_minutes(minutes) == text


@pytest.mark.parametrize("value, workers", [("8", 8), ("1", 1), ("", 1), ("0", 1), ("many", 1)])
def test_jobs_give_the_workers_the_estimate_divides_by(value: str, workers: int) -> None:
    assert run_plan.parse_jobs(value) == workers


def test_the_plan_text_shows_checkboxes_skipped_groups_totals_and_the_footer() -> None:
    built = plan(skipped=["summon"])
    lines = run_plan.format_plan(built, (True, False), run_plan.RunContext(launcher="local", jobs_text="1", shard="2/8"))
    text = "\n".join(lines)
    assert "launcher local" in text and "JOBS 1" in text and "shard 2/8" in text
    assert "[x] 1. boot" in text
    assert "[ ] 2. poison" in text
    assert "skipped: summon (1 test)" in text
    # Unticking poison leaves only test_one: 30 seconds.
    assert "Total: 1 test, about 0.5 min (estimate from recorded times)" in text
    assert "even one another ticked group also marks" in text


def test_the_plan_text_counts_the_kept_tests_without_a_recorded_time() -> None:
    lines = run_plan.format_plan(plan(), (True, True, True), run_plan.RunContext(launcher="docker", jobs_text="1", shard=None))
    text = "\n".join(lines)
    assert "shard" not in text
    assert "2 tests have no recorded time and count 15 s each" in text


# Expressions and commands


@pytest.mark.parametrize("group, excluded, expression", [
    ("", [], "scenario"),
    ("boot or (poison and not summon)", [], "scenario and (boot or (poison and not summon))"),
    ("", ["poison", "summon"], "scenario and not (poison or summon)"),
    ("boot or poison", ["summon"], "scenario and (boot or poison) and not (summon)"),
])
def test_the_run_expression_keeps_the_group_and_leaves_out_the_excluded(group: str, excluded: list[str], expression: str) -> None:
    assert run_plan.run_expression(group, excluded) == expression


def test_the_run_command_is_todays_pytest_command() -> None:
    assert run_plan.pytest_run_command("py", "scenario and (boot)", "8") == TODAYS_BOOT_COMMAND


@pytest.mark.parametrize("jobs_text", ["", "0", "1"])
def test_one_worker_runs_without_xdist(jobs_text: str) -> None:
    assert "-n" not in run_plan.pytest_run_command("py", "scenario", jobs_text)


def test_the_repeat_line_reproduces_the_choice() -> None:
    line = run_plan.repeat_command(".venv/bin/python", "boot or poison", ["summon", "shaping"], "8")
    assert line == "make integration PYTHON=.venv/bin/python GROUP='boot or poison' JOBS=8 SKIP_GROUPS='summon shaping' CONFIRM=yes"


def test_the_repeat_line_leaves_out_defaults() -> None:
    assert run_plan.repeat_command("python3", "", ["summon"], "1") == "make integration SKIP_GROUPS=summon CONFIRM=yes"


# Answers


@pytest.mark.parametrize("answer, ticked", [
    ("2", (True, False, True)),
    ("1 3", (False, True, False)),
    ("1,3", (False, True, False)),
    ("2-3", (True, False, False)),
    ("a", (True, True, True)),
])
def test_answers_toggle_and_tick_groups(answer: str, ticked: tuple[bool, ...]) -> None:
    result = run_plan.apply_answer(answer, (True, True, True))
    assert result.outcome is AnswerOutcome.CONTINUE
    assert result.ticked == ticked


def test_n_unticks_every_group() -> None:
    assert run_plan.apply_answer("n", (True, False)).ticked == (False, False)


def test_toggling_twice_ticks_again() -> None:
    once = run_plan.apply_answer("2", (True, True))
    assert run_plan.apply_answer("2", once.ticked).ticked == (True, True)


@pytest.mark.parametrize("answer", ["", "y", "Y", "yes"])
def test_enter_or_y_starts(answer: str) -> None:
    result = run_plan.apply_answer(answer, (True, False))
    assert result.outcome is AnswerOutcome.START
    assert result.ticked == (True, False)


def test_q_cancels() -> None:
    assert run_plan.apply_answer("q", (True,)).outcome is AnswerOutcome.CANCEL


def test_starting_with_nothing_ticked_is_refused() -> None:
    result = run_plan.apply_answer("", (False, False))
    assert result.outcome is AnswerOutcome.CONTINUE
    assert result.message is not None and "nothing is ticked" in result.message


@pytest.mark.parametrize("answer", ["0", "4", "3-2", "x", "1-", "2 zebra"])
def test_bad_answers_change_nothing_and_say_why(answer: str) -> None:
    result = run_plan.apply_answer(answer, (True, True, True))
    assert result.outcome is AnswerOutcome.CONTINUE
    assert result.ticked == (True, True, True)
    assert result.message


# Gate decision


def test_ci_is_recognised_from_ci_true() -> None:
    assert run_plan.is_ci({"CI": "true"})
    assert run_plan.is_ci({"CI": "TRUE"})
    assert not run_plan.is_ci({})
    assert not run_plan.is_ci({"CI": "false"})


def test_confirm_yes_runs_without_asking_however_long() -> None:
    assert run_plan.decide_gate(confirm=True, interactive=True, estimate_minutes=500, threshold_minutes=10) is Gate.RUN


def test_a_terminal_asks_even_for_a_short_run() -> None:
    assert run_plan.decide_gate(confirm=False, interactive=True, estimate_minutes=1, threshold_minutes=10) is Gate.ASK


def test_without_a_terminal_a_short_run_starts() -> None:
    assert run_plan.decide_gate(confirm=False, interactive=False, estimate_minutes=9.9, threshold_minutes=10) is Gate.RUN


def test_without_a_terminal_a_long_run_needs_confirmation() -> None:
    assert run_plan.decide_gate(confirm=False, interactive=False, estimate_minutes=10, threshold_minutes=10) is Gate.NEEDS_CONFIRMATION


def test_the_confirm_threshold_defaults_to_ten_minutes() -> None:
    assert run_plan.parse_confirm_minutes(None) == 10.0
    assert run_plan.parse_confirm_minutes("2.5") == 2.5


@pytest.mark.parametrize("value", ["0", "-1", "abc", "", "nan", "inf"])
def test_a_threshold_that_is_not_a_positive_number_is_rejected(value: str) -> None:
    with pytest.raises(ValueError):
        run_plan.parse_confirm_minutes(value)


def test_confirm_accepts_yes_or_nothing() -> None:
    assert run_plan.parse_confirm("yes") is True
    assert run_plan.parse_confirm("YES") is True
    assert run_plan.parse_confirm("") is False
    with pytest.raises(ValueError):
        run_plan.parse_confirm("no")


# Timed prompt


class ScriptedReader:
    """Answers read(timeout) from a script: None stands for a timeout. Records each timeout asked for."""

    def __init__(self, answers: list[Optional[str]]) -> None:
        # The answers still to give, in order.
        self._answers = list(answers)
        # The timeout of every read, in order.
        self.timeouts: list[Optional[float]] = []
        # Whether a read is outstanding; the scripted reader never leaves one.
        self.reading = False

    def read(self, timeout: Optional[float]) -> Optional[str]:
        self.timeouts.append(timeout)
        return self._answers.pop(0)


def ask(reader, timeout_seconds: float = 300.0, step_seconds: float = 60.0) -> tuple[run_plan.PromptResult, list[str]]:
    written: list[str] = []
    result = run_plan.ask(
        3, lambda ticked: [f"plan {ticked}"], reader, written.append, timeout_seconds=timeout_seconds, countdown_step_seconds=step_seconds,
    )
    return result, written


def test_no_input_starts_with_every_group_ticked_after_the_timeout() -> None:
    blocked = threading.Event()
    reader = run_plan.TimedLineReader(lambda: (blocked.wait(), "")[1])
    result, written = ask(reader, timeout_seconds=0.05, step_seconds=0.02)
    assert result.outcome is AnswerOutcome.START
    assert result.ticked == (True, True, True)
    assert result.timed_out
    assert reader.reading
    assert any("starting" in line.lower() for line in written)
    blocked.set()


def test_the_countdown_prints_while_it_waits() -> None:
    reader = ScriptedReader([None, None, "q\n"])
    _, written = ask(reader, timeout_seconds=300.0, step_seconds=60.0)
    assert sum("starts by itself in" in line for line in written) == 2


def test_the_first_input_stops_the_countdown_for_good() -> None:
    reader = ScriptedReader(["2\n", "\n"])
    result, written = ask(reader)
    assert result.outcome is AnswerOutcome.START
    assert result.ticked == (True, False, True)
    assert not result.timed_out
    assert reader.timeouts[0] is not None
    assert reader.timeouts[1] is None
    assert "plan (True, False, True)" in written


def test_a_refused_start_keeps_asking() -> None:
    reader = ScriptedReader(["n\n", "\n", "q\n"])
    result, written = ask(reader)
    assert result.outcome is AnswerOutcome.CANCEL
    assert any("nothing is ticked" in line for line in written)


def test_closed_input_cancels() -> None:
    result, _ = ask(ScriptedReader([""]))
    assert result.outcome is AnswerOutcome.CANCEL


def test_the_timed_reader_returns_a_line_and_leaves_no_read_outstanding() -> None:
    reader = run_plan.TimedLineReader(iter(["first\n", "second\n"]).__next__)
    assert reader.read(1.0) == "first\n"
    assert not reader.reading
    assert reader.read(1.0) == "second\n"


def test_the_timed_reader_keeps_a_late_line_for_the_next_read() -> None:
    release = threading.Event()
    reader = run_plan.TimedLineReader(lambda: (release.wait(), "late\n")[1])
    assert reader.read(0.01) is None
    assert reader.reading
    release.set()
    assert reader.read(1.0) == "late\n"


def test_the_timed_reader_reports_a_failing_source_as_closed_input() -> None:
    def closed() -> str:
        raise ValueError("I/O operation on closed file")

    assert run_plan.TimedLineReader(closed).read(1.0) == ""


# main


class FakeRunner:
    """Records the commands main runs and answers each with a status."""

    def __init__(self, status: int = 0) -> None:
        # The status every command returns.
        self._status = status
        # Each command run, with the stdin it was given.
        self.commands: list[tuple[list[str], bool]] = []

    def __call__(self, command: list[str], keep_stdin: bool) -> int:
        self.commands.append((command, keep_stdin))
        return self._status


def fail_collection(python: str, group: str, environ) -> tuple[Optional[list[CollectedTest]], int]:
    raise AssertionError("main collected tests")


def run_main(arguments: list[str], environ: dict[str, str], runner: FakeRunner, tmp_path: Path, collect=fail_collection, interactive: bool = False) -> int:
    return run_plan.main(
        arguments, environ=environ, run_command=runner, collect=collect, interactive=interactive, durations_path=tmp_path / "durations.json",
    )


def test_under_ci_main_runs_todays_command_without_collecting(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    runner = FakeRunner(status=3)
    status = run_main(["--python", "py", "--group", "boot", "--jobs", "8", "--skip-groups", "poison"], {"CI": "true"}, runner, tmp_path)
    assert status == 3
    assert runner.commands == [(TODAYS_BOOT_COMMAND, True)]
    assert capsys.readouterr().out == ""


def test_an_unknown_skip_group_is_a_usage_error(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    runner = FakeRunner()
    assert run_main(["--python", "py", "--skip-groups", "bot"], {}, runner, tmp_path) == run_plan.EXIT_USAGE
    assert runner.commands == []
    assert "bot" in capsys.readouterr().err


def test_a_bad_threshold_is_a_usage_error(tmp_path: Path) -> None:
    runner = FakeRunner()
    environ = {run_plan.CONFIRM_MINUTES_VARIABLE: "soon"}
    assert run_main(["--python", "py"], environ, runner, tmp_path) == run_plan.EXIT_USAGE
    assert runner.commands == []


def test_a_bad_confirm_is_a_usage_error(tmp_path: Path) -> None:
    assert run_main(["--python", "py", "--confirm", "no"], {}, FakeRunner(), tmp_path) == run_plan.EXIT_USAGE


def collect_boot_tests(python: str, group: str, environ) -> tuple[Optional[list[CollectedTest]], int]:
    return [test for test in TESTS if "boot" in test.markers], 0


def collect_forty_tests(python: str, group: str, environ) -> tuple[Optional[list[CollectedTest]], int]:
    # 40 untimed tests at 15 seconds: 10 minutes on one worker.
    return [CollectedTest(f"scenarios/test_many.py::test_{index}", frozenset({"scenario", "boot"})) for index in range(40)], 0


def test_without_a_terminal_a_short_run_starts_at_once(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    runner = FakeRunner()
    assert run_main(["--python", "py", "--group", "boot"], {}, runner, tmp_path, collect=collect_boot_tests) == 0
    assert [command for command, _ in runner.commands] == [run_plan.pytest_run_command("py", "scenario and (boot)", "")]
    assert "[x] 1. boot" in capsys.readouterr().out


def test_without_a_terminal_a_long_run_stops_with_its_own_status(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    runner = FakeRunner()
    assert run_main(["--python", "py"], {}, runner, tmp_path, collect=collect_forty_tests) == run_plan.EXIT_NEEDS_CONFIRMATION
    assert runner.commands == []
    assert "CONFIRM=yes" in capsys.readouterr().out


def test_a_higher_threshold_lets_the_long_run_start(tmp_path: Path) -> None:
    runner = FakeRunner()
    environ = {run_plan.CONFIRM_MINUTES_VARIABLE: "11"}
    assert run_main(["--python", "py"], environ, runner, tmp_path, collect=collect_forty_tests) == 0
    assert len(runner.commands) == 1


def test_confirm_yes_runs_the_long_run_without_the_skipped_groups_and_prints_the_repeat_line(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    runner = FakeRunner()
    status = run_main(["--python", "py", "--confirm", "yes", "--skip-groups", "poison"], {}, runner, tmp_path, collect=collect_forty_tests)
    assert status == 0
    assert [command for command, _ in runner.commands] == [run_plan.pytest_run_command("py", "scenario and not (poison)", "")]
    assert "Repeat: make integration PYTHON=py SKIP_GROUPS=poison CONFIRM=yes" in capsys.readouterr().out


def test_plan_only_prints_the_plan_and_runs_nothing(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    runner = FakeRunner()
    assert run_main(["--python", "py", "--plan-only"], {"CI": "true"}, runner, tmp_path, collect=collect_forty_tests) == 0
    assert runner.commands == []
    assert "Total: 40 tests, about 10 min" in capsys.readouterr().out


def test_a_failed_collection_returns_its_status(tmp_path: Path) -> None:
    runner = FakeRunner()

    def failed(python: str, group: str, environ) -> tuple[Optional[list[CollectedTest]], int]:
        return None, 4

    assert run_main(["--python", "py", "--group", "bot"], {}, runner, tmp_path, collect=failed) == 4
    assert runner.commands == []


def test_an_empty_selection_runs_pytest_at_once(tmp_path: Path) -> None:
    runner = FakeRunner(status=5)

    def nothing(python: str, group: str, environ) -> tuple[Optional[list[CollectedTest]], int]:
        return [], 5

    assert run_main(["--python", "py"], {}, runner, tmp_path, collect=nothing, interactive=True) == 5
    assert len(runner.commands) == 1


def test_at_a_terminal_an_unticked_group_is_left_out_and_the_repeat_line_printed(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    runner = FakeRunner()

    def collect_all(python: str, group: str, environ) -> tuple[Optional[list[CollectedTest]], int]:
        return TESTS, 0

    reader = ScriptedReader(["2\n", "y\n"])
    status = run_plan.main(
        ["--python", "py"], environ={}, run_command=runner, collect=collect_all, interactive=True, reader=reader,
        durations_path=tmp_path / "durations.json",
    )
    assert status == 0
    assert runner.commands == [(run_plan.pytest_run_command("py", "scenario and not (poison)", ""), True)]
    assert "Repeat: make integration PYTHON=py SKIP_GROUPS=poison CONFIRM=yes" in capsys.readouterr().out


def test_at_a_terminal_cancel_runs_nothing_and_returns_its_own_status(tmp_path: Path) -> None:
    runner = FakeRunner()
    status = run_plan.main(
        ["--python", "py"], environ={}, run_command=runner, collect=collect_boot_tests, interactive=True, reader=ScriptedReader(["q\n"]),
        durations_path=tmp_path / "durations.json",
    )
    assert status == run_plan.EXIT_CANCELLED
    assert runner.commands == []


def test_a_run_started_by_the_countdown_gets_no_terminal_input(tmp_path: Path) -> None:
    runner = FakeRunner()
    blocked = threading.Event()
    reader = run_plan.TimedLineReader(lambda: (blocked.wait(), "")[1])
    status = run_plan.main(
        ["--python", "py"], environ={}, run_command=runner, collect=collect_boot_tests, interactive=True, reader=reader,
        durations_path=tmp_path / "durations.json", prompt_timeout_seconds=0.05, countdown_step_seconds=0.02,
    )
    assert status == 0
    assert [keep_stdin for _, keep_stdin in runner.commands] == [False]
    blocked.set()


def test_main_estimates_from_the_durations_file(tmp_path: Path, capsys: pytest.CaptureFixture) -> None:
    durations_path = tmp_path / "durations.json"
    durations_path.write_text('{"scenarios/test_many.py::test_0": 1200}', encoding="utf-8")
    status = run_plan.main(
        ["--python", "py", "--plan-only"], environ={}, run_command=FakeRunner(), collect=collect_forty_tests, interactive=False,
        durations_path=durations_path,
    )
    assert status == 0
    # 1200 + 39 * 15 seconds.
    assert "Total: 40 tests, about 30 min" in capsys.readouterr().out


def test_ctrl_c_at_the_prompt_cancels(tmp_path: Path) -> None:
    class InterruptedReader(ScriptedReader):
        def read(self, timeout: Optional[float]) -> Optional[str]:
            raise KeyboardInterrupt

    runner = FakeRunner()
    status = run_plan.main(
        ["--python", "py"], environ={}, run_command=runner, collect=collect_boot_tests, interactive=True, reader=InterruptedReader([]),
        durations_path=tmp_path / "durations.json",
    )
    assert status == run_plan.EXIT_CANCELLED
    assert runner.commands == []
