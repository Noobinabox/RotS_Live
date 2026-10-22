"""Shows the scenario groups a local `make integration` would run, asks about them, and runs pytest with the groups kept.

The plan lists each selected group with its test count and estimated minutes, from the times
build/integration/durations.json recorded. At a terminal every group starts ticked: the developer
can untick groups, start or cancel, and an unanswered plan starts by itself after 5 minutes.
Without a terminal a run estimated under ROTS_IT_CONFIRM_MINUTES (default 10) starts at once and
a longer one stops with EXIT_NEEDS_CONFIRMATION unless CONFIRM=yes. Under CI=true it runs the
pytest command make integration always ran, and prints nothing of its own.

Run from the repository root as `make integration` or `make integration-plan`.
"""

from __future__ import annotations

import argparse
import enum
import math
import os
import platform
import queue
import re
import shlex
import signal
import subprocess
import sys
import tempfile
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterable, Mapping, Optional, Sequence

from rots_harness import collect_report, durations
from rots_harness import groups as group_files
from rots_harness.collect_report import CollectedTest

# tests/integration: the groups folder, the scenarios and the rots_harness package.
INTEGRATION_ROOT = Path(__file__).resolve().parents[1]
# The checkout whose scenarios run; every pytest command runs here.
REPO_ROOT = INTEGRATION_ROOT.parents[1]
# One <group>.txt file per registered scenario group.
GROUPS_DIR = INTEGRATION_ROOT / group_files.GROUPS_DIRECTORY_NAME
# The scenarios folder, relative to the repository root, as make integration names it.
SCENARIOS_RELATIVE = "tests/integration/scenarios"
# The pytest arguments writing each test's result and time, and listing the slowest tests.
REPORT_ARGUMENTS = ("--junitxml=build/integration/results.xml", "--durations=25")

# A SKIP_GROUPS, CONFIRM or ROTS_IT_CONFIRM_MINUTES value that cannot be used, or an unreadable group file.
EXIT_USAGE = 2
# No terminal to ask at, and the run's estimate reaches ROTS_IT_CONFIRM_MINUTES without CONFIRM=yes.
# Above pytest's own statuses (0 to 5), so a caller can tell it apart from a test result.
EXIT_NEEDS_CONFIRMATION = 10
# The developer cancelled the run at the prompt; nothing ran.
EXIT_CANCELLED = 11

# Seconds counted for a test with no recorded time; CI's measured average was 14.8.
DEFAULT_TEST_SECONDS = 15.0
# The environment variable holding the estimate, in minutes, from which a run without a terminal needs CONFIRM=yes.
CONFIRM_MINUTES_VARIABLE = "ROTS_IT_CONFIRM_MINUTES"
# The confirmation threshold when CONFIRM_MINUTES_VARIABLE is unset.
DEFAULT_CONFIRM_MINUTES = 10.0
# Seconds an unanswered prompt waits before the run starts with the groups still ticked.
PROMPT_TIMEOUT_SECONDS = 300.0
# Seconds between the countdown lines of an unanswered prompt.
COUNTDOWN_STEP_SECONDS = 60.0
# The CONFIRM value that starts a run without asking.
CONFIRM_YES = "yes"
# JOBS values make integration runs without pytest-xdist.
SINGLE_WORKER_JOBS = frozenset({"", "0", "1"})
# JOBS values pytest-xdist reads as the number of CPUs.
CPU_COUNT_JOBS = frozenset({"auto", "logical"})
# The PYTHON the Makefile uses when none is given; the repeat line leaves it out.
DEFAULT_PYTHON = "python3"
# What separates names in SKIP_GROUPS and numbers in an answer.
SEPARATOR_PATTERN = re.compile(r"[\s,]+")
# A group number or a range of them in an answer, such as 3 or 3-5.
GROUP_RANGE_PATTERN = re.compile(r"(\d+)(?:-(\d+))?")
# The answers that start the run.
START_ANSWERS = frozenset({"", "y", "yes"})
# The prompt's one line of help.
PROMPT_HELP = "Numbers (and ranges like 3-5) toggle groups, a ticks all, n unticks all, Enter or y starts, q cancels."
# The plan's footer about tests in several groups.
UNTICK_FOOTER = "Unticking a group drops every test it marks, even one another ticked group also marks."


@dataclass(frozen=True)
class PlannedGroup:
    """A group with at least one selected test."""

    # The group's marker name.
    name: str
    # The node ids of the selected tests it marks, in collection order.
    test_ids: tuple[str, ...]


@dataclass
class RunPlan:
    """The selected tests, the groups they fall in, and what the recorded times say the run will take."""

    # Every selected test, in collection order.
    tests: list[CollectedTest]
    # The groups offered as checkboxes, in registration order; a group no selected test is in is left out.
    offered_groups: list[PlannedGroup]
    # The SKIP_GROUPS groups with a selected test, in registration order.
    skipped_groups: list[PlannedGroup]
    # Every SKIP_GROUPS name; their tests never run.
    skipped_names: frozenset[str]
    # Recorded seconds by node id.
    recorded_seconds: Mapping[str, float]
    # The workers the run is divided between, at least 1.
    workers: int
    # Selected tests that no registered group marks.
    ungrouped_count: int = 0

    def kept_test_ids(self, unticked: Iterable[str]) -> list[str]:
        """The node ids of the selected tests no skipped or unticked group marks, in collection order."""
        excluded = self.skipped_names | set(unticked)
        return [test.node_id for test in self.tests if not test.markers & excluded]

    def estimated_minutes(self, test_ids: Iterable[str]) -> float:
        """Minutes the tests take on the run's workers: recorded seconds, or DEFAULT_TEST_SECONDS for a test without."""
        seconds = sum(self.recorded_seconds.get(node_id, DEFAULT_TEST_SECONDS) for node_id in test_ids)
        return seconds / self.workers / 60

    def untimed_count(self, test_ids: Iterable[str]) -> int:
        """How many of the tests have no recorded time."""
        return sum(1 for node_id in test_ids if node_id not in self.recorded_seconds)


@dataclass(frozen=True)
class RunContext:
    """What the plan's heading reports about how the run is made."""

    # The launcher ROTS_IT_LAUNCHER selects.
    launcher: str
    # JOBS as make integration passes it.
    jobs_text: str
    # ROTS_IT_SHARD, or None when unset.
    shard: Optional[str]


class AnswerOutcome(enum.Enum):
    """What an answer at the prompt does."""

    # Keep asking, with the ticks the answer left.
    CONTINUE = "continue"
    # Run the ticked groups.
    START = "start"
    # Run nothing.
    CANCEL = "cancel"


@dataclass(frozen=True)
class AnswerResult:
    """An answer's outcome, the ticks after it, and a message for an answer that changed nothing."""

    # What happens next.
    outcome: AnswerOutcome
    # One tick per offered group, in plan order.
    ticked: tuple[bool, ...]
    # Why the answer changed nothing, or None.
    message: Optional[str] = None


@dataclass(frozen=True)
class PromptResult:
    """How the prompt ended."""

    # START or CANCEL.
    outcome: AnswerOutcome
    # One tick per offered group, in plan order.
    ticked: tuple[bool, ...]
    # Whether the run starts because nobody answered in time.
    timed_out: bool = False


class Gate(enum.Enum):
    """What happens once the plan is shown."""

    # Run without asking.
    RUN = "run"
    # Ask at the terminal.
    ASK = "ask"
    # Stop with EXIT_NEEDS_CONFIRMATION.
    NEEDS_CONFIRMATION = "needs confirmation"


def parse_skip_groups(value: str, registered: Sequence[str]) -> list[str]:
    """The group names in SKIP_GROUPS, separated by spaces or commas, without repeats; raises ValueError for an unregistered name."""
    names: list[str] = []
    for name in SEPARATOR_PATTERN.split(value.strip()):
        if name and name not in names:
            names.append(name)
    unknown = [name for name in names if name not in registered]
    if unknown:
        raise ValueError(f"SKIP_GROUPS names {', '.join(unknown)}, which no group file registers; registered: {', '.join(registered)}")
    return names


def parse_confirm(value: str) -> bool:
    """Whether CONFIRM says yes; raises ValueError for anything but yes or nothing."""
    if value.strip().lower() == CONFIRM_YES:
        return True
    if not value.strip():
        return False
    raise ValueError(f"CONFIRM must be {CONFIRM_YES} or empty, not {value!r}")


def parse_confirm_minutes(value: Optional[str]) -> float:
    """The confirmation threshold in minutes from ROTS_IT_CONFIRM_MINUTES, or the default when unset; raises ValueError."""
    if value is None:
        return DEFAULT_CONFIRM_MINUTES
    problem = ValueError(f"{CONFIRM_MINUTES_VARIABLE} must be a positive number of minutes, not {value!r}")
    try:
        minutes = float(value)
    except ValueError:
        raise problem from None
    if not math.isfinite(minutes) or minutes <= 0:
        raise problem
    return minutes


def parse_jobs(value: str) -> int:
    """The workers a JOBS value runs the tests on, for the estimate: 1 for a value pytest-xdist would not run in parallel."""
    text = value.strip()
    if text in CPU_COUNT_JOBS:
        return os.cpu_count() or 1
    try:
        return max(int(text), 1)
    except ValueError:
        return 1


def is_ci(environ: Mapping[str, str]) -> bool:
    """Whether CI=true, which GitHub Actions sets."""
    return environ.get("CI", "").lower() == "true"


def launcher_name(environ: Mapping[str, str]) -> str:
    """The launcher the run uses, as conftest.launcher_mode chooses it; that function checks the value."""
    return environ.get("ROTS_IT_LAUNCHER") or ("local" if platform.system() == "Linux" else "docker")


def build_plan(
    tests: Iterable[CollectedTest], registered: Sequence[str], skipped: Iterable[str], recorded_seconds: Mapping[str, float], workers: int,
) -> RunPlan:
    """The plan for the collected tests: a test counts toward every registered group it is marked with."""
    test_list = list(tests)
    skipped_names = frozenset(skipped)
    offered: list[PlannedGroup] = []
    skipped_groups: list[PlannedGroup] = []
    for name in registered:
        test_ids = tuple(test.node_id for test in test_list if name in test.markers)
        if not test_ids:
            continue
        (skipped_groups if name in skipped_names else offered).append(PlannedGroup(name, test_ids))
    registered_names = set(registered)
    ungrouped = sum(1 for test in test_list if not test.markers & registered_names)
    return RunPlan(test_list, offered, skipped_groups, skipped_names, dict(recorded_seconds), max(workers, 1), ungrouped)


def count_text(count: int) -> str:
    """'1 test' or 'N tests'."""
    return f"{count} test" if count == 1 else f"{count} tests"


def format_minutes(minutes: float) -> str:
    """Minutes to one decimal below 10, whole minutes from 10."""
    if minutes < 10:
        return f"{minutes:.1f} min"
    return f"{int(minutes + 0.5)} min"


def format_plan(plan: RunPlan, ticked: Sequence[bool], context: RunContext) -> list[str]:
    """The plan's lines: a heading, one checkbox per offered group, the skipped groups, the total and the footer."""
    heading = f"Integration run plan: launcher {context.launcher}, JOBS {context.jobs_text or '1'}"
    if context.shard:
        heading += f", shard {context.shard}"
    lines = [heading]
    shown = plan.offered_groups + plan.skipped_groups
    name_width = max((len(group.name) for group in shown), default=0)
    number_width = len(str(len(plan.offered_groups)))
    for number, (group, is_ticked) in enumerate(zip(plan.offered_groups, ticked), start=1):
        box = "x" if is_ticked else " "
        minutes = format_minutes(plan.estimated_minutes(group.test_ids))
        lines.append(f"  [{box}] {number:>{number_width}}. {group.name:<{name_width}}  {count_text(len(group.test_ids)):>9}  {minutes:>8}")
    for group in plan.skipped_groups:
        lines.append(f"  skipped: {group.name} ({count_text(len(group.test_ids))})")
    if plan.ungrouped_count:
        lines.append(f"  {count_text(plan.ungrouped_count)} in no group")
    unticked = [group.name for group, is_ticked in zip(plan.offered_groups, ticked) if not is_ticked]
    kept = plan.kept_test_ids(unticked)
    untimed = plan.untimed_count(kept)
    if untimed:
        verb = "has" if untimed == 1 else "have"
        note = f"estimate; {count_text(untimed)} {verb} no recorded time and count {DEFAULT_TEST_SECONDS:g} s each"
    else:
        note = "estimate from recorded times"
    lines.append(f"Total: {count_text(len(kept))}, about {format_minutes(plan.estimated_minutes(kept))} ({note}).")
    if plan.offered_groups:
        lines.append(UNTICK_FOOTER)
    return lines


def run_expression(group: str, excluded: Sequence[str]) -> str:
    """The -m expression: the scenarios in GROUP (every scenario when empty), without any test an excluded group marks."""
    expression = "scenario"
    if group:
        expression += f" and ({group})"
    if excluded:
        expression += f" and not ({' or '.join(excluded)})"
    return expression


def pytest_run_command(python: str, expression: str, jobs_text: str) -> list[str]:
    """The pytest command make integration runs, with -n only for more than one worker."""
    command = [python, "-m", "pytest", SCENARIOS_RELATIVE, "-q", "-m", expression]
    if jobs_text.strip() not in SINGLE_WORKER_JOBS:
        command += ["-n", jobs_text.strip()]
    return command + list(REPORT_ARGUMENTS)


def collect_command(python: str, group: str) -> list[str]:
    """The pytest command that collects GROUP's scenarios, sharding included, and reports them through collect_report."""
    return [python, "-m", "pytest", SCENARIOS_RELATIVE, "-q", "--collect-only", "-p", collect_report.__name__, "-m", run_expression(group, [])]


def repeat_command(python: str, group: str, excluded: Sequence[str], jobs_text: str) -> str:
    """The make integration command that repeats a run leaving out the excluded groups, without asking."""
    parts = ["make", "integration"]
    if python != DEFAULT_PYTHON:
        parts.append(f"PYTHON={shlex.quote(python)}")
    if group:
        parts.append(f"GROUP={shlex.quote(group)}")
    if jobs_text.strip() not in SINGLE_WORKER_JOBS:
        parts.append(f"JOBS={shlex.quote(jobs_text.strip())}")
    parts.append(f"SKIP_GROUPS={shlex.quote(' '.join(excluded))}")
    parts.append(f"CONFIRM={CONFIRM_YES}")
    return " ".join(parts)


def apply_answer(text: str, ticked: Sequence[bool]) -> AnswerResult:
    """What one line typed at the prompt does to the ticks.

    Numbers and ranges toggle groups (numbered from 1), a ticks all, n unticks all, Enter or y
    starts and q cancels. Starting with nothing ticked, or an answer with any part that is not one
    of these, changes nothing and says why.
    """
    current = tuple(ticked)
    answer = text.strip().lower()
    if answer in START_ANSWERS:
        if not any(current):
            return AnswerResult(AnswerOutcome.CONTINUE, current, "nothing is ticked: tick a group, or q to cancel")
        return AnswerResult(AnswerOutcome.START, current)
    if answer == "q":
        return AnswerResult(AnswerOutcome.CANCEL, current)
    if answer == "a":
        return AnswerResult(AnswerOutcome.CONTINUE, (True,) * len(current))
    if answer == "n":
        return AnswerResult(AnswerOutcome.CONTINUE, (False,) * len(current))
    toggled = list(current)
    for token in SEPARATOR_PATTERN.split(answer):
        if not token:
            continue
        match = GROUP_RANGE_PATTERN.fullmatch(token)
        first = int(match.group(1)) if match else 0
        last = int(match.group(2) or first) if match else 0
        if not match or first < 1 or last < first or last > len(current):
            return AnswerResult(AnswerOutcome.CONTINUE, current, f"{token!r} is not a group number from 1 to {len(current)} or a range such as 2-3")
        for index in range(first - 1, last):
            toggled[index] = not toggled[index]
    return AnswerResult(AnswerOutcome.CONTINUE, tuple(toggled))


def decide_gate(*, confirm: bool, interactive: bool, estimate_minutes: float, threshold_minutes: float) -> Gate:
    """Run when CONFIRM=yes, ask at a terminal, and otherwise run only an estimate under the threshold."""
    if confirm:
        return Gate.RUN
    if interactive:
        return Gate.ASK
    if estimate_minutes < threshold_minutes:
        return Gate.RUN
    return Gate.NEEDS_CONFIRMATION


class TimedLineReader:
    """Reads lines from a blocking source on a daemon thread, so a caller can wait for one with a timeout.

    The thread reads one line per request, never ahead: once a read has returned, nothing reads
    the source until the next request. A read that timed out stays outstanding (`reading`) and
    its line is kept for the next read; standard Python cannot cancel a blocked readline.
    """

    def __init__(self, read_line: Callable[[], str]) -> None:
        # Returns the next line, or "" at the end of input.
        self._read_line = read_line
        # One entry per line asked for.
        self._requests: queue.Queue[None] = queue.Queue()
        # The lines read, in order.
        self._lines: queue.Queue[str] = queue.Queue()
        # Whether a line was asked for and not yet returned.
        self.reading = False
        threading.Thread(target=self._serve, name="run-plan-input", daemon=True).start()

    def _serve(self) -> None:
        """Reads one line per request; a source that fails reads as the end of input."""
        while True:
            self._requests.get()
            try:
                line = self._read_line()
            except (OSError, ValueError):
                line = ""
            self._lines.put(line)

    def read(self, timeout: Optional[float]) -> Optional[str]:
        """The next line ("" at the end of input), or None when timeout seconds pass first; None waits for ever."""
        if not self.reading:
            self._requests.put(None)
            self.reading = True
        try:
            line = self._lines.get(timeout=timeout)
        except queue.Empty:
            return None
        self.reading = False
        return line


def format_wait(seconds: float) -> str:
    """A wait in whole minutes, or in seconds below one minute."""
    if seconds >= 60:
        return f"{math.ceil(seconds / 60)} min"
    return f"{math.ceil(seconds)} s"


def ask(
    group_count: int,
    render: Callable[[tuple[bool, ...]], list[str]],
    reader,
    write: Callable[[str], None],
    *,
    timeout_seconds: float,
    countdown_step_seconds: float,
) -> PromptResult:
    """Asks which of group_count groups to run, starting with all of them ticked.

    render gives the plan's lines for a set of ticks; reader.read(timeout) gives the next line,
    "" at the end of input, or None after timeout seconds. Until the first answer a line counts
    down each countdown_step_seconds, and the run starts with the groups still ticked once
    timeout_seconds pass; after it the prompt waits for ever. The end of input cancels.
    """
    ticked = (True,) * group_count
    for line in render(ticked):
        write(line)
    write(PROMPT_HELP)
    write(f"With no answer in {format_wait(timeout_seconds)}, the run starts.")
    deadline = time.monotonic() + timeout_seconds
    answered = False
    while True:
        if answered:
            line = reader.read(None)
        else:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                write(f"No answer in {format_wait(timeout_seconds)}: starting.")
                return PromptResult(AnswerOutcome.START, ticked, timed_out=True)
            line = reader.read(min(countdown_step_seconds, remaining))
            if line is None:
                remaining = deadline - time.monotonic()
                if remaining > 0:
                    write(f"No answer yet: the run starts by itself in {format_wait(remaining)}.")
                continue
            answered = True
        if line == "":
            write("Input closed: cancelled.")
            return PromptResult(AnswerOutcome.CANCEL, ticked)
        result = apply_answer(line, ticked)
        if result.outcome is not AnswerOutcome.CONTINUE:
            return PromptResult(result.outcome, result.ticked)
        if result.message:
            write(result.message)
            continue
        ticked = result.ticked
        for plan_line in render(ticked):
            write(plan_line)


def run_in_foreground(command: list[str], keep_stdin: bool) -> int:
    """Runs command in the repository root and returns its exit status; stdin is the terminal's only when keep_stdin.

    pytest handles Ctrl-C, SIGTERM and SIGHUP itself, stopping its servers and releasing the
    Docker lock, so this process ignores them while it waits rather than cutting that short.
    They reach pytest when they are sent to the process group, as a terminal sends them. The
    handlers change only once pytest has started, so pytest does not inherit them.
    """
    process = subprocess.Popen(command, cwd=REPO_ROOT, stdin=None if keep_stdin else subprocess.DEVNULL)
    previous_handlers = {}
    for signal_name in ("SIGINT", "SIGTERM", "SIGHUP"):
        signal_number = getattr(signal, signal_name, None)
        if signal_number is not None:
            previous_handlers[signal_number] = signal.signal(signal_number, signal.SIG_IGN)
    try:
        return process.wait()
    finally:
        for signal_number, handler in previous_handlers.items():
            signal.signal(signal_number, handler)


def collect_selection(python: str, group: str, environ: Mapping[str, str]) -> tuple[Optional[list[CollectedTest]], int]:
    """The tests a run of GROUP selects, with the environment's ROTS_IT_SHARD applied, and pytest's exit status.

    The tests are None when collection fails; pytest's output is then printed to stderr.
    """
    with tempfile.TemporaryDirectory(prefix="rots-run-plan-") as folder:
        report_path = Path(folder) / "collected.json"
        child_environment = dict(environ)
        child_environment[collect_report.REPORT_PATH_VARIABLE] = str(report_path)
        # The plugin is imported before pytest.ini's pythonpath applies.
        child_environment["PYTHONPATH"] = os.pathsep.join(filter(None, [str(INTEGRATION_ROOT), environ.get("PYTHONPATH")]))
        completed = subprocess.run(
            collect_command(python, group), cwd=REPO_ROOT, env=child_environment, stdin=subprocess.DEVNULL, capture_output=True, text=True, check=False,
        )
        # 5: nothing collected. The plugin still reports the empty selection.
        if completed.returncode in (0, 5):
            try:
                return collect_report.read_report(report_path), completed.returncode
            except (OSError, ValueError, KeyError, TypeError):
                pass
    sys.stderr.write(completed.stdout)
    sys.stderr.write(completed.stderr)
    print(f"error: collecting the selection failed (pytest exit status {completed.returncode})", file=sys.stderr)
    return None, completed.returncode or EXIT_USAGE


def _print_lines(lines: Iterable[str]) -> None:
    """Prints each line and flushes, so the plan shows before anything a child process prints."""
    for line in lines:
        print(line)
    sys.stdout.flush()


def _write_line(line: str) -> None:
    """Prints one line at once."""
    print(line, flush=True)


def main(
    argv: Optional[list[str]] = None,
    *,
    environ: Optional[Mapping[str, str]] = None,
    run_command: Callable[[list[str], bool], int] = run_in_foreground,
    collect: Callable[[str, str, Mapping[str, str]], tuple[Optional[list[CollectedTest]], int]] = collect_selection,
    interactive: Optional[bool] = None,
    reader=None,
    durations_path: Optional[Path] = None,
    prompt_timeout_seconds: float = PROMPT_TIMEOUT_SECONDS,
    countdown_step_seconds: float = COUNTDOWN_STEP_SECONDS,
) -> int:
    """Shows the plan, applies the gate and returns pytest's exit status, or EXIT_USAGE, EXIT_NEEDS_CONFIRMATION or EXIT_CANCELLED.

    A failed collection returns pytest's status. --plan-only shows the plan and returns 0, CI or not.
    The keyword arguments replace the environment, the terminal and the subprocesses in tests.
    """
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--python", required=True, help="the Python that runs pytest")
    parser.add_argument("--group", default="", help="GROUP: a group or -m expression of groups; empty selects every scenario")
    parser.add_argument("--skip-groups", default="", help="SKIP_GROUPS: groups to leave out, separated by spaces or commas")
    parser.add_argument("--confirm", default="", help=f"CONFIRM: {CONFIRM_YES} starts the run without asking")
    parser.add_argument("--jobs", default="", help="the pytest-xdist workers; empty, 0 or 1 runs without them")
    parser.add_argument("--plan-only", action="store_true", help="print the plan and exit 0 without asking or running")
    arguments = parser.parse_args(argv)
    environment = os.environ if environ is None else environ
    if not arguments.plan_only and is_ci(environment):
        return run_command(pytest_run_command(arguments.python, run_expression(arguments.group, []), arguments.jobs), True)
    try:
        registered = list(group_files.read_group_descriptions(GROUPS_DIR))
        skipped = parse_skip_groups(arguments.skip_groups, registered)
        confirm = parse_confirm(arguments.confirm)
        threshold_minutes = parse_confirm_minutes(environment.get(CONFIRM_MINUTES_VARIABLE))
    except (ValueError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return EXIT_USAGE
    tests, status = collect(arguments.python, arguments.group, environment)
    if tests is None:
        return status
    recorded = durations.read_durations(durations_path or REPO_ROOT / durations.DURATIONS_RELATIVE)
    plan = build_plan(tests, registered, skipped, recorded, parse_jobs(arguments.jobs))
    context = RunContext(launcher_name(environment), arguments.jobs, environment.get("ROTS_IT_SHARD") or None)

    def render(ticked: tuple[bool, ...]) -> list[str]:
        return format_plan(plan, ticked, context)

    ticked = (True,) * len(plan.offered_groups)
    if arguments.plan_only:
        _print_lines(render(ticked))
        return 0
    keep_stdin = True
    estimate = plan.estimated_minutes(plan.kept_test_ids([]))
    if interactive is None:
        interactive = sys.stdin.isatty() and sys.stdout.isatty()
    gate = decide_gate(confirm=confirm, interactive=interactive, estimate_minutes=estimate, threshold_minutes=threshold_minutes)
    if gate is Gate.ASK and plan.offered_groups:
        line_reader = reader if reader is not None else TimedLineReader(sys.stdin.readline)
        try:
            result = ask(
                len(plan.offered_groups), render, line_reader, _write_line, timeout_seconds=prompt_timeout_seconds, countdown_step_seconds=countdown_step_seconds,
            )
        except KeyboardInterrupt:
            # Ctrl-C at the prompt cancels like q.
            result = PromptResult(AnswerOutcome.CANCEL, ticked)
        if result.outcome is AnswerOutcome.CANCEL:
            _write_line(f"Cancelled; nothing ran (exit status {EXIT_CANCELLED}).")
            return EXIT_CANCELLED
        ticked = result.ticked
        # A readline still waiting would take a line typed for pytest (a breakpoint's pdb, say).
        keep_stdin = not line_reader.reading
        if not keep_stdin:
            _write_line("The run started by itself, so it reads no input from this terminal.")
    else:
        _print_lines(render(ticked))
        if gate is Gate.NEEDS_CONFIRMATION and plan.tests:
            _print_lines([
                f"This run is estimated at about {format_minutes(estimate)}, reaching {CONFIRM_MINUTES_VARIABLE} ({threshold_minutes:g} min), "
                "and there is no terminal to ask at.",
                f"Start it with CONFIRM={CONFIRM_YES}, or choose fewer groups with GROUP= or SKIP_GROUPS= (exit status {EXIT_NEEDS_CONFIRMATION}).",
            ])
            return EXIT_NEEDS_CONFIRMATION
    unticked = [group.name for group, is_ticked in zip(plan.offered_groups, ticked) if not is_ticked]
    excluded = skipped + [name for name in unticked if name not in skipped]
    status = run_command(pytest_run_command(arguments.python, run_expression(arguments.group, excluded), arguments.jobs), keep_stdin)
    if excluded:
        _write_line(f"Repeat: {repeat_command(arguments.python, arguments.group, excluded, arguments.jobs)}")
    return status


if __name__ == "__main__":
    sys.exit(main())
