from __future__ import annotations

import io
import shlex
import subprocess
import sys
import time
from pathlib import Path
from typing import Iterator, List, Optional

import pytest

from rots_harness import launcher


def test_local_launcher_builds_the_server_command(tmp_path: Path) -> None:
    local = launcher.LocalProcessLauncher(repo_root=tmp_path, binary_relative="bin/ageland")
    lib_dir = tmp_path / "run" / "lib"
    command = local.command(lib_dir=lib_dir, port=4321, seed=7)
    assert command == [str(tmp_path / "bin" / "ageland"), "--random-seed", "7", "-d", str(lib_dir), "4321"]


def test_docker_launcher_maps_run_dir_into_the_container_and_publishes_a_random_port(tmp_path: Path) -> None:
    repo_root = tmp_path
    run_dir = repo_root / "build" / "integration" / "abc123"
    docker = launcher.DockerComposeLauncher(repo_root=repo_root, binary_relative="bin/ageland")
    command = docker.server_command(run_dir=run_dir, lib_dir=run_dir / "lib", port=4321, container_name="rots-it-abc123", seed=7)
    assert command[:5] == ["docker", "compose", "run", "--rm", "-T"]
    assert "--name" in command and command[command.index("--name") + 1] == "rots-it-abc123"
    assert "-p" in command and command[command.index("-p") + 1] == "127.0.0.1::4321"
    assert "TMPDIR=/rots/build/integration/tmp" in command
    assert command[command.index("TMPDIR=/rots/build/integration/tmp") - 1] == "-e"
    assert "--service-ports" not in command
    shell_script = command[-1]
    assert "ulimit -c 0;" in shell_script
    assert "cd /rots/build/integration/abc123" in shell_script
    assert "exec /rots/bin/ageland --random-seed 7 -d /rots/build/integration/abc123/lib 4321" in shell_script


def test_container_path_refuses_a_path_outside_the_repo(tmp_path: Path) -> None:
    docker = launcher.DockerComposeLauncher(repo_root=tmp_path / "repo")
    with pytest.raises(ValueError, match="outside the repository"):
        docker.container_path(tmp_path / "elsewhere" / "lib")


def test_docker_tool_command_runs_the_program_in_the_mapped_directory(tmp_path: Path) -> None:
    docker = launcher.DockerComposeLauncher(repo_root=tmp_path)
    command = docker.tool_command("bin/rotstool", ["fixtures", "write", "--lib", "lib", "spec.json"], tmp_path / "build" / "integration" / "libs" / "k1", "rots-it-tool1")
    assert command[:5] == ["docker", "compose", "run", "--rm", "-T"]
    assert "-p" not in command
    assert command[-1] == "ulimit -c 0; cd /rots/build/integration/libs/k1 && exec /rots/bin/rotstool fixtures write --lib lib spec.json"


def test_local_run_to_completion_returns_status_and_combined_output(tmp_path: Path) -> None:
    program = tmp_path / "bin" / "tool"
    program.parent.mkdir()
    program.write_text("#!/bin/sh\necho out; echo err >&2; exit 3\n", encoding="utf-8")
    program.chmod(0o700)
    result = launcher.LocalProcessLauncher(repo_root=tmp_path).run_to_completion("bin/tool", [], tmp_path, timeout=10)
    assert result.exit_status == 3
    assert "out" in result.output and "err" in result.output


def test_require_program_names_the_build_command_when_missing(tmp_path: Path) -> None:
    with pytest.raises(RuntimeError) as missing:
        launcher.LocalProcessLauncher(repo_root=tmp_path).require_program("bin/rotstool")
    assert str(missing.value) == "bin/rotstool is missing; build it first: make configure && cmake --build build --target ageland rotstool"


def test_docker_require_program_names_the_build_command_inside_the_container(tmp_path: Path) -> None:
    with pytest.raises(RuntimeError) as missing:
        launcher.DockerComposeLauncher(repo_root=tmp_path).require_program("bin/ageland")
    assert str(missing.value) == (
        "bin/ageland is missing; build it first: "
        "docker compose run --rm -T rots bash -lc 'make configure && cmake --build build --target ageland rotstool'"
    )


def test_local_start_gives_the_server_a_standard_input_pipe(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    recorded_kwargs: list[dict] = []

    class StopAfterRecording(Exception):
        pass

    def recording_popen(*args: object, **kwargs: object):
        recorded_kwargs.append(kwargs)
        raise StopAfterRecording()

    binary = tmp_path / "bin" / "ageland"
    binary.parent.mkdir()
    binary.write_text("", encoding="utf-8")
    monkeypatch.setattr(launcher.subprocess, "Popen", recording_popen)
    run_dir = tmp_path / "run"
    run_dir.mkdir()
    with pytest.raises(StopAfterRecording):
        launcher.LocalProcessLauncher(repo_root=tmp_path).start(run_dir, run_dir / "lib", port=4321, seed=1)
    assert recorded_kwargs[0]["stdin"] == launcher.subprocess.PIPE


def test_allocate_free_port_returns_a_high_port() -> None:
    port = launcher.allocate_free_port()
    assert 1024 < port < 65536


def test_local_launcher_kills_the_process_when_the_port_never_opens(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    stalling_script = tmp_path / "bin" / "ageland"
    stalling_script.parent.mkdir()
    stalling_script.write_text("#!/bin/sh\nexec sleep 60\n", encoding="utf-8")
    stalling_script.chmod(0o700)
    run_dir = tmp_path / "run"
    run_dir.mkdir()
    lib_dir = run_dir / "lib"
    lib_dir.mkdir()

    recorded_processes: list[subprocess.Popen] = []
    real_popen = launcher.subprocess.Popen

    def recording_popen(*args: object, **kwargs: object) -> subprocess.Popen:
        spawned_process = real_popen(*args, **kwargs)
        recorded_processes.append(spawned_process)
        return spawned_process

    monkeypatch.setattr(launcher.subprocess, "Popen", recording_popen)

    local = launcher.LocalProcessLauncher(repo_root=tmp_path, binary_relative="bin/ageland", startup_timeout=1.0)
    with pytest.raises(RuntimeError):
        local.start(run_dir, lib_dir, port=launcher.allocate_free_port(), seed=1)

    assert len(recorded_processes) == 1
    assert recorded_processes[0].poll() is not None


def test_docker_launcher_stops_the_container_when_start_fails(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    class FakeComposeProcess:
        def __init__(self) -> None:
            # Never set: the fake compose process is always still running.
            self.returncode: int | None = None
            # Whether start's cleanup asked the process to terminate.
            self.terminate_called = False
            # Whether start's cleanup escalated to a kill.
            self.kill_called = False
            # Stands in for the pipe start opens; cleanup must close it.
            self.stdin = io.BytesIO()

        def poll(self) -> int | None:
            return None

        def terminate(self) -> None:
            self.terminate_called = True

        def kill(self) -> None:
            self.kill_called = True

        def wait(self, timeout: float | None = None) -> int:
            return 0

    # A placeholder binary keeps start past require_program, so the failure is the unpublished port.
    binary = tmp_path / "bin" / "ageland"
    binary.parent.mkdir()
    binary.write_text("", encoding="utf-8")
    run_dir = tmp_path / "run"
    run_dir.mkdir()
    lib_dir = run_dir / "lib"
    lib_dir.mkdir()

    fake_process = FakeComposeProcess()
    recorded_argv: list[list[str]] = []

    def fake_run(argv: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
        recorded_argv.append(argv)
        return subprocess.CompletedProcess(argv, 1, "", "")

    monkeypatch.setattr(launcher.subprocess, "Popen", lambda *args, **kwargs: fake_process)
    monkeypatch.setattr(launcher.subprocess, "run", fake_run)

    docker = launcher.DockerComposeLauncher(repo_root=tmp_path, binary_relative="bin/ageland", startup_timeout=0.2)
    with pytest.raises(RuntimeError):
        docker.start(run_dir, lib_dir, port=4321, seed=1)

    cleanup_calls = [argv for argv in recorded_argv if argv[:2] != ["docker", "port"]]
    assert len(cleanup_calls) == 2
    container_name = cleanup_calls[0][-1]
    assert container_name.startswith("rots-it-")
    assert cleanup_calls == [["docker", "stop", "-t", "5", container_name], ["docker", "rm", "-f", container_name]]
    assert fake_process.terminate_called or fake_process.kill_called
    assert fake_process.stdin.closed


def test_local_launcher_kills_the_process_when_interrupted_while_waiting(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    stalling_script = tmp_path / "bin" / "ageland"
    stalling_script.parent.mkdir()
    stalling_script.write_text("#!/bin/sh\nexec sleep 60\n", encoding="utf-8")
    stalling_script.chmod(0o700)
    run_dir = tmp_path / "run"
    run_dir.mkdir()

    recorded_processes: list[subprocess.Popen] = []
    real_popen = launcher.subprocess.Popen

    def recording_popen(*args: object, **kwargs: object) -> subprocess.Popen:
        spawned_process = real_popen(*args, **kwargs)
        recorded_processes.append(spawned_process)
        return spawned_process

    def interrupted_wait(*args: object, **kwargs: object) -> None:
        raise KeyboardInterrupt()

    monkeypatch.setattr(launcher.subprocess, "Popen", recording_popen)
    monkeypatch.setattr(launcher, "wait_for_login_prompt", interrupted_wait)

    local = launcher.LocalProcessLauncher(repo_root=tmp_path, binary_relative="bin/ageland")
    with pytest.raises(KeyboardInterrupt):
        local.start(run_dir, run_dir / "lib", port=launcher.allocate_free_port(), seed=1)

    assert len(recorded_processes) == 1
    assert recorded_processes[0].poll() is not None


def test_docker_launcher_stops_the_container_when_interrupted_while_waiting(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    binary = tmp_path / "bin" / "ageland"
    binary.parent.mkdir()
    binary.write_text("", encoding="utf-8")
    run_dir = tmp_path / "run"
    run_dir.mkdir()

    compose_process = subprocess.Popen(["sleep", "60"], stdin=subprocess.PIPE)
    recorded_argv: list[list[str]] = []

    def interrupted_port_query(argv: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
        recorded_argv.append(argv)
        if argv[:2] == ["docker", "port"]:
            raise KeyboardInterrupt()
        return subprocess.CompletedProcess(argv, 0, "", "")

    monkeypatch.setattr(launcher.subprocess, "Popen", lambda *args, **kwargs: compose_process)
    monkeypatch.setattr(launcher.subprocess, "run", interrupted_port_query)

    docker = launcher.DockerComposeLauncher(repo_root=tmp_path, binary_relative="bin/ageland")
    try:
        with pytest.raises(KeyboardInterrupt):
            docker.start(run_dir, run_dir / "lib", port=4321, seed=1)

        stop_calls = [argv for argv in recorded_argv if argv[:4] == ["docker", "stop", "-t", "5"]]
        assert len(stop_calls) == 1
        assert stop_calls[0][4].startswith("rots-it-")
        assert compose_process.poll() is not None
    finally:
        if compose_process.poll() is None:
            compose_process.kill()
            compose_process.wait()


def test_docker_run_to_completion_stops_the_container_when_interrupted(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    program = tmp_path / "bin" / "rotstool"
    program.parent.mkdir()
    program.write_text("", encoding="utf-8")
    recorded_argv: list[list[str]] = []

    def interrupted_compose_run(argv: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
        recorded_argv.append(argv)
        if argv[:3] == ["docker", "compose", "run"]:
            raise KeyboardInterrupt()
        return subprocess.CompletedProcess(argv, 0, "", "")

    monkeypatch.setattr(launcher.subprocess, "run", interrupted_compose_run)

    docker = launcher.DockerComposeLauncher(repo_root=tmp_path)
    with pytest.raises(KeyboardInterrupt):
        docker.run_to_completion("bin/rotstool", [], tmp_path, timeout=10)

    cleanup_calls = recorded_argv[1:]
    assert len(cleanup_calls) == 2
    container_name = cleanup_calls[0][-1]
    assert container_name.startswith("rots-it-tool-")
    assert cleanup_calls == [["docker", "stop", "-t", "5", container_name], ["docker", "rm", "-f", container_name]]


def test_local_launcher_environment_is_clean_apart_from_sanitizer_tuning(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("ASAN_OPTIONS", "detect_leaks=0:handle_segv=2")
    monkeypatch.setenv("ASAN_SYMBOLIZER_PATH", "/usr/bin/llvm-symbolizer")
    monkeypatch.delenv("LSAN_OPTIONS", raising=False)
    monkeypatch.delenv("UBSAN_OPTIONS", raising=False)
    # A host-only harness variable must not reach the server.
    monkeypatch.setenv("ROTS_IT_SEED", "5")

    environment = launcher.LocalProcessLauncher(repo_root=tmp_path).environment()

    assert environment["ASAN_OPTIONS"] == "detect_leaks=0:handle_segv=2"
    assert environment["ASAN_SYMBOLIZER_PATH"] == "/usr/bin/llvm-symbolizer"
    assert "LSAN_OPTIONS" not in environment
    assert "UBSAN_OPTIONS" not in environment
    assert "ROTS_IT_SEED" not in environment
    assert set(environment) <= {"PATH", "HOME", *launcher.SANITIZER_ENVIRONMENT_VARIABLES}


def test_local_launcher_environment_has_no_sanitizer_keys_when_the_host_sets_none(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    for name in launcher.SANITIZER_ENVIRONMENT_VARIABLES:
        monkeypatch.delenv(name, raising=False)

    environment = launcher.LocalProcessLauncher(repo_root=tmp_path).environment()

    assert set(environment) == {"PATH", "HOME"}


def test_read_log_tail_returns_only_the_last_bytes_and_defaults_to_the_documented_size(tmp_path: Path) -> None:
    log_path = tmp_path / "game.log"
    log_path.write_bytes(b"x" * (launcher.LOG_TAIL_BYTES + 100) + b"TAIL")

    assert launcher.read_log_tail(log_path, max_bytes=8) == "xxxxTAIL"
    assert len(launcher.read_log_tail(log_path)) == launcher.LOG_TAIL_BYTES
    assert launcher.LOG_TAIL_BYTES == 16000


def test_terminate_process_warns_instead_of_raising_when_the_process_survives_sigkill(
    capsys: pytest.CaptureFixture[str],
) -> None:
    class UnreapableProcess:
        def __init__(self) -> None:
            # Reported in the warning that names the surviving process.
            self.pid = 4242
            # Never set: the process never exits.
            self.returncode: int | None = None
            # How many times terminate_process asked the process to terminate.
            self.terminate_calls = 0
            # How many times terminate_process escalated to a kill.
            self.kill_calls = 0
            # Stands in for the server's stdin pipe; closed even though the process survives.
            self.stdin = io.BytesIO()

        def poll(self) -> int | None:
            return None

        def terminate(self) -> None:
            self.terminate_calls += 1

        def kill(self) -> None:
            self.kill_calls += 1

        def wait(self, timeout: float | None = None) -> int:
            raise subprocess.TimeoutExpired(cmd="ageland", timeout=timeout or 0)

    stuck_process = UnreapableProcess()

    launcher.terminate_process(stuck_process)  # type: ignore[arg-type]

    assert stuck_process.terminate_calls == 1
    assert stuck_process.kill_calls == 1, "terminate_process must escalate to SIGKILL exactly once"
    assert "survived SIGKILL" in capsys.readouterr().err
    assert stuck_process.stdin.closed


class ExitedProcess:
    """A server process that has already exited, holding the stdin pipe its launcher opened."""

    def __init__(self) -> None:
        # Stands in for the pipe the launcher opened; stop must close it.
        self.stdin = io.BytesIO()
        # The exit status poll reports.
        self.returncode = 0

    def poll(self) -> int:
        return self.returncode

    def wait(self, timeout: float | None = None) -> int:
        return self.returncode


def test_local_stop_closes_the_server_standard_input(tmp_path: Path) -> None:
    exited_process = ExitedProcess()
    handle = launcher.ServerHandle(launcher.LOOPBACK, 4321, tmp_path / "game.log", exited_process)  # type: ignore[arg-type]

    launcher.LocalProcessLauncher(repo_root=tmp_path).stop(handle)

    assert exited_process.stdin.closed


def test_docker_stop_closes_the_server_standard_input(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(launcher.subprocess, "run", lambda argv, **kwargs: subprocess.CompletedProcess(argv, 0, "", ""))
    exited_process = ExitedProcess()
    handle = launcher.ServerHandle(launcher.LOOPBACK, 4321, tmp_path / "game.log", exited_process, "rots-it-stop")  # type: ignore[arg-type]

    launcher.DockerComposeLauncher(repo_root=tmp_path).stop(handle)

    assert exited_process.stdin.closed


# A stand-in for ageland's listener: argv is the port, then a directory for the files the test reads.
FAKE_SERVER_PREAMBLE = """
import socket
import sys
import time
from pathlib import Path

port = int(sys.argv[1])
evidence_dir = Path(sys.argv[2])
listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
listener.bind(("127.0.0.1", port))
listener.listen(5)
print("fake server listening", flush=True)
# Telnet negotiation (IAC WILL ECHO) comes before the greeting, as from the real server.
NEGOTIATION = bytes([255, 251, 1])
"""

# Accepts at once but sends the prompt, split across two writes, only after a delay.
DELAYED_PROMPT_SERVER = FAKE_SERVER_PREAMBLE + """
connection, _address = listener.accept()
time.sleep(0.5)
(evidence_dir / "prompt-sent").write_text("yes")
connection.sendall(NEGOTIATION + b"Welcome\\r\\n\\r\\nAccount em")
time.sleep(0.2)
connection.sendall(b"ail: ")
time.sleep(30)
"""

# Closes the first connection unanswered, as a port proxy does before the server listens.
CLOSES_FIRST_CONNECTION_SERVER = FAKE_SERVER_PREAMBLE + """
first_connection, _address = listener.accept()
first_connection.close()
second_connection, _address = listener.accept()
(evidence_dir / "connections").write_text("2")
second_connection.sendall(NEGOTIATION + b"Account email: ")
time.sleep(30)
"""

# Accepts connections and never sends anything.
SILENT_SERVER = FAKE_SERVER_PREAMBLE + """
held_connections = []
while True:
    connection, _address = listener.accept()
    held_connections.append(connection)
"""

# Fails during boot without ever listening.
EXITS_BEFORE_LISTENING_SERVER = """
import sys
print("SYSERR: boot failed: zone 999 is missing", flush=True)
sys.exit(3)
"""


class FakeServers:
    """Starts fake server scripts with this test's Python and kills any still running at teardown."""

    def __init__(self, work_dir: Path) -> None:
        # Holds each script, its log and the evidence files it writes.
        self._work_dir = work_dir
        # Every process started, killed at teardown if still running.
        self._processes: List[subprocess.Popen] = []

    def start(self, source: str, port: int) -> subprocess.Popen:
        """Runs source as a script with port and the evidence directory as arguments, logging to fake.log."""
        script = self._work_dir / f"fake_server_{len(self._processes)}.py"
        script.write_text(source, encoding="utf-8")
        with self.log_path.open("ab") as log_file:
            process = subprocess.Popen(
                [sys.executable, str(script), str(port), str(self._work_dir)], stdin=subprocess.DEVNULL, stdout=log_file, stderr=subprocess.STDOUT,
            )
        self._processes.append(process)
        return process

    @property
    def log_path(self) -> Path:
        """The log every fake server writes to."""
        return self._work_dir / "fake.log"

    def kill_all(self) -> None:
        """Kills every started process that is still running."""
        for process in self._processes:
            if process.poll() is None:
                process.kill()
            process.wait()


@pytest.fixture
def fake_servers(tmp_path: Path) -> Iterator[FakeServers]:
    """Fake servers for one test, all killed when it ends, whatever its outcome."""
    servers = FakeServers(tmp_path)
    try:
        yield servers
    finally:
        servers.kill_all()


def test_ready_waits_for_the_login_prompt_not_the_listening_port(tmp_path: Path, fake_servers: FakeServers) -> None:
    port = launcher.allocate_free_port()
    process = fake_servers.start(DELAYED_PROMPT_SERVER, port)

    launcher.wait_for_login_prompt(launcher.LOOPBACK, port, 10.0, process, fake_servers.log_path)

    assert (tmp_path / "prompt-sent").exists()


def test_ready_retries_a_connection_closed_before_the_prompt(tmp_path: Path, fake_servers: FakeServers) -> None:
    port = launcher.allocate_free_port()
    process = fake_servers.start(CLOSES_FIRST_CONNECTION_SERVER, port)

    launcher.wait_for_login_prompt(launcher.LOOPBACK, port, 10.0, process, fake_servers.log_path)

    assert (tmp_path / "connections").read_text() == "2"


def test_ready_reports_a_server_that_exits_before_listening_with_its_log_tail(fake_servers: FakeServers) -> None:
    port = launcher.allocate_free_port()
    process = fake_servers.start(EXITS_BEFORE_LISTENING_SERVER, port)

    with pytest.raises(RuntimeError) as raised:
        launcher.wait_for_login_prompt(launcher.LOOPBACK, port, 10.0, process, fake_servers.log_path)

    message = str(raised.value)
    assert message.startswith("server exited with status 3 before it was ready")
    assert "boot failed: zone 999 is missing" in message


def test_ready_gives_up_on_a_server_that_never_sends_its_prompt(fake_servers: FakeServers) -> None:
    port = launcher.allocate_free_port()
    process = fake_servers.start(SILENT_SERVER, port)

    started = time.monotonic()
    with pytest.raises(RuntimeError) as raised:
        launcher.wait_for_login_prompt(launcher.LOOPBACK, port, 1.5, process, fake_servers.log_path)

    assert time.monotonic() - started < 5.0
    message = str(raised.value)
    assert message.startswith(f"the server on {launcher.LOOPBACK}:{port} did not send its login prompt within 1.5s")
    assert "fake server listening" in message


def test_the_session_waits_for_the_same_login_prompt_as_the_launcher() -> None:
    from rots_harness import session

    assert launcher.LOGIN_EMAIL_PROMPT == "Account email:"
    assert session.LOGIN_EMAIL_PROMPT is launcher.LOGIN_EMAIL_PROMPT


class FakeComposeClient:
    """A `docker compose run` client that records stop's calls in a shared event list."""

    def __init__(self, events: list, waits_out_the_first_wait: bool = False) -> None:
        # The ordered record of docker commands and process calls the test inspects.
        self._events = events
        # Whether the first wait times out, as a client that outlives docker stop does.
        self._waits_out_the_first_wait = waits_out_the_first_wait
        # How many times wait was called.
        self._wait_calls = 0
        # None while the client runs; set by kill or a successful wait.
        self.returncode: Optional[int] = None
        # Stands in for the pipe start opens; stop must close it.
        self.stdin = io.BytesIO()
        # Named in stop's warning if the client survives its kill.
        self.pid = 4242

    def poll(self) -> Optional[int]:
        return self.returncode

    def wait(self, timeout: Optional[float] = None) -> int:
        self._events.append(("wait", timeout))
        self._wait_calls += 1
        if self._waits_out_the_first_wait and self._wait_calls == 1:
            raise subprocess.TimeoutExpired(cmd="docker", timeout=timeout or 0)
        self.returncode = 0
        return 0

    def kill(self) -> None:
        self._events.append(("kill",))
        self.returncode = -9


def recording_docker(events: list, failing_command: Optional[str] = None, failure: Optional[BaseException] = None):
    """A subprocess.run stand-in that records each docker argv; failing_command fails with failure or status 1."""

    def fake_run(argv: List[str], **kwargs: object) -> subprocess.CompletedProcess:
        events.append(("run", argv))
        if argv[1] == failing_command:
            if failure is not None:
                raise failure
            return subprocess.CompletedProcess(argv, 1, b"", b"")
        return subprocess.CompletedProcess(argv, 0, b"", b"")

    return fake_run


def test_docker_stop_stops_the_container_then_waits_for_the_client(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    events: list = []
    monkeypatch.setattr(launcher.subprocess, "run", recording_docker(events))
    client = FakeComposeClient(events)
    handle = launcher.ServerHandle(launcher.LOOPBACK, 4321, tmp_path / "game.log", client, "rots-it-stop")  # type: ignore[arg-type]

    launcher.DockerComposeLauncher(repo_root=tmp_path).stop(handle)

    assert events == [("run", ["docker", "stop", "-t", "5", "rots-it-stop"]), ("wait", 30)]
    assert client.stdin.closed


def test_docker_stop_kills_a_client_that_outlives_the_wait_and_removes_the_container(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    events: list = []
    monkeypatch.setattr(launcher.subprocess, "run", recording_docker(events))
    client = FakeComposeClient(events, waits_out_the_first_wait=True)
    handle = launcher.ServerHandle(launcher.LOOPBACK, 4321, tmp_path / "game.log", client, "rots-it-stop")  # type: ignore[arg-type]

    launcher.DockerComposeLauncher(repo_root=tmp_path).stop(handle)

    assert events[:3] == [("run", ["docker", "stop", "-t", "5", "rots-it-stop"]), ("wait", 30), ("kill",)]
    post_kill_wait = events[3]
    assert post_kill_wait[0] == "wait" and post_kill_wait[1] is not None and post_kill_wait[1] > 0
    assert events[4:] == [("run", ["docker", "rm", "-f", "rots-it-stop"])]
    assert client.stdin.closed


@pytest.mark.parametrize(
    "failure",
    [None, subprocess.TimeoutExpired(cmd="docker", timeout=15), FileNotFoundError("docker")],
    ids=["non-zero-exit", "timeout", "docker-missing"],
)
def test_docker_stop_removes_the_container_when_docker_stop_fails(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, failure: Optional[BaseException]
) -> None:
    events: list = []
    monkeypatch.setattr(launcher.subprocess, "run", recording_docker(events, failing_command="stop", failure=failure))
    client = FakeComposeClient(events)
    handle = launcher.ServerHandle(launcher.LOOPBACK, 4321, tmp_path / "game.log", client, "rots-it-stop")  # type: ignore[arg-type]

    launcher.DockerComposeLauncher(repo_root=tmp_path).stop(handle)

    assert events[:2] == [("run", ["docker", "stop", "-t", "5", "rots-it-stop"]), ("run", ["docker", "rm", "-f", "rots-it-stop"])]
    assert client.stdin.closed


@pytest.mark.parametrize(
    "failure",
    [None, subprocess.TimeoutExpired(cmd="docker", timeout=15), FileNotFoundError("docker")],
    ids=["non-zero-exit", "timeout", "docker-missing"],
)
def test_a_failed_docker_rm_is_ignored(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, failure: Optional[BaseException]) -> None:
    events: list = []
    monkeypatch.setattr(launcher.subprocess, "run", recording_docker(events, failing_command="rm", failure=failure))
    docker = launcher.DockerComposeLauncher(repo_root=tmp_path)

    docker._remove_container("rots-it-gone")

    assert events == [("run", ["docker", "rm", "-f", "rots-it-gone"])]


def test_docker_reads_the_published_port_from_docker_port(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    recorded_argv: List[List[str]] = []

    def fake_port_query(argv: List[str], **kwargs: object) -> subprocess.CompletedProcess:
        recorded_argv.append(argv)
        return subprocess.CompletedProcess(argv, 0, "127.0.0.1:49153\n", "")

    monkeypatch.setattr(launcher.subprocess, "run", fake_port_query)
    docker = launcher.DockerComposeLauncher(repo_root=tmp_path)

    host_port = docker._wait_for_published_port("rots-it-port", 4321, FakeComposeClient([]), tmp_path / "game.log", time.monotonic() + 5.0)  # type: ignore[arg-type]

    assert host_port == 49153
    assert recorded_argv == [["docker", "port", "rots-it-port", "4321/tcp"]]


def test_docker_runs_the_container_as_the_host_user(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(launcher.os, "getuid", lambda: 1234)
    monkeypatch.setattr(launcher.os, "getgid", lambda: 5678)

    environment = launcher.DockerComposeLauncher(repo_root=tmp_path)._docker_environment()

    assert environment["ROTS_UID"] == "1234"
    assert environment["ROTS_GID"] == "5678"


@pytest.mark.parametrize("launcher_class", [launcher.LocalProcessLauncher, launcher.DockerComposeLauncher])
def test_run_to_completion_gives_the_program_no_standard_input(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, launcher_class: type
) -> None:
    program = tmp_path / "bin" / "rotstool"
    program.parent.mkdir()
    program.write_text("", encoding="utf-8")
    recorded_kwargs: List[dict] = []

    def recording_run(argv: List[str], **kwargs: object) -> subprocess.CompletedProcess:
        recorded_kwargs.append(kwargs)
        return subprocess.CompletedProcess(argv, 0, b"done", None)

    monkeypatch.setattr(launcher.subprocess, "run", recording_run)

    result = launcher_class(repo_root=tmp_path).run_to_completion("bin/rotstool", [], tmp_path, timeout=10)

    assert result == launcher.ToolResult(0, "done")
    assert recorded_kwargs[0]["stdin"] == subprocess.DEVNULL


def write_program_with_a_missing_interpreter(repo_root: Path, program_relative: str) -> Path:
    """An executable script whose interpreter does not exist, as an i386 binary looks without its loader."""
    program = repo_root / program_relative
    program.parent.mkdir(parents=True, exist_ok=True)
    program.write_text("#!/nonexistent/i386-loader\n", encoding="utf-8")
    program.chmod(0o700)
    return program


def test_local_run_to_completion_explains_a_program_this_host_cannot_execute(tmp_path: Path) -> None:
    program = write_program_with_a_missing_interpreter(tmp_path, "bin/rotstool")

    with pytest.raises(RuntimeError) as raised:
        launcher.LocalProcessLauncher(repo_root=tmp_path).run_to_completion("bin/rotstool", [], tmp_path, timeout=10)

    assert str(raised.value) == (
        f"cannot execute {program}: this host cannot run the i386 binaries; "
        "install the 32-bit runtime (e.g. libc6:i386) or set ROTS_IT_LAUNCHER=docker"
    )


def test_local_start_explains_a_server_this_host_cannot_execute(tmp_path: Path) -> None:
    program = write_program_with_a_missing_interpreter(tmp_path, "bin/ageland")
    run_dir = tmp_path / "run"
    run_dir.mkdir()

    with pytest.raises(RuntimeError, match=f"cannot execute {program}: this host cannot run the i386 binaries"):
        launcher.LocalProcessLauncher(repo_root=tmp_path).start(run_dir, run_dir / "lib", port=4321, seed=1)


def test_local_run_to_completion_in_a_missing_directory_is_not_blamed_on_the_host(tmp_path: Path) -> None:
    program = tmp_path / "bin" / "tool"
    program.parent.mkdir()
    program.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
    program.chmod(0o700)

    with pytest.raises(FileNotFoundError):
        launcher.LocalProcessLauncher(repo_root=tmp_path).run_to_completion("bin/tool", [], tmp_path / "missing", timeout=10)


@pytest.mark.parametrize("launcher_class", [launcher.LocalProcessLauncher, launcher.DockerComposeLauncher])
@pytest.mark.parametrize("binary_relative", ["/usr/local/bin/ageland", "../ageland", "bin/../../ageland"])
def test_launchers_refuse_a_binary_outside_the_repo(tmp_path: Path, launcher_class: type, binary_relative: str) -> None:
    with pytest.raises(ValueError, match="relative to the repository root"):
        launcher_class(repo_root=tmp_path, binary_relative=binary_relative)


def test_docker_server_script_quotes_every_path(tmp_path: Path) -> None:
    run_dir = tmp_path / "build" / "integration" / "run dir"
    docker = launcher.DockerComposeLauncher(repo_root=tmp_path, binary_relative="bin/age land")

    command = docker.server_command(run_dir=run_dir, lib_dir=run_dir / "lib", port=4321, container_name="rots-it-q", seed=7)

    assert shlex.split(command[-1]) == [
        "ulimit", "-c", "0;", "cd", "/rots/build/integration/run dir", "&&",
        "exec", "/rots/bin/age land", "--random-seed", "7", "-d", "/rots/build/integration/run dir/lib", "4321",
    ]


def test_docker_tool_script_quotes_every_path(tmp_path: Path) -> None:
    docker = launcher.DockerComposeLauncher(repo_root=tmp_path)

    command = docker.tool_command("bin/rots tool", ["fixtures", "write"], tmp_path / "build" / "work dir", "rots-it-tool-q")

    assert shlex.split(command[-1]) == [
        "ulimit", "-c", "0;", "cd", "/rots/build/work dir", "&&", "exec", "/rots/bin/rots tool", "fixtures", "write",
    ]


class FakeClock:
    """Stands in for the time module: monotonic() reads a settable value and sleep() advances it."""

    def __init__(self, now: float) -> None:
        # The value monotonic() returns.
        self.now = now

    def monotonic(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


def test_docker_start_gives_both_waits_one_startup_deadline(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    clock = FakeClock(now=1000.0)
    monkeypatch.setattr(launcher, "time", clock)
    monkeypatch.setattr(launcher.subprocess, "Popen", lambda *args, **kwargs: FakeComposeClient([]))
    binary = tmp_path / "bin" / "ageland"
    binary.parent.mkdir()
    binary.write_text("", encoding="utf-8")
    run_dir = tmp_path / "run"
    run_dir.mkdir()
    docker = launcher.DockerComposeLauncher(repo_root=tmp_path, startup_timeout=300.0)
    port_wait_deadlines: List[float] = []
    login_wait_timeouts: List[float] = []

    def slow_port_wait(container_name: str, container_port: int, process: object, log_path: Path, deadline: float) -> int:
        port_wait_deadlines.append(deadline)
        clock.now += 200.0
        return 49153

    def recording_login_wait(host: str, port: int, timeout_seconds: float, process: object, log_path: Path, time_allowed: Optional[str] = None) -> None:
        login_wait_timeouts.append(timeout_seconds)

    monkeypatch.setattr(docker, "_wait_for_published_port", slow_port_wait)
    monkeypatch.setattr(launcher, "wait_for_login_prompt", recording_login_wait)

    handle = docker.start(run_dir, run_dir / "lib", port=4321, seed=1)

    assert handle.port == 49153
    assert port_wait_deadlines == [1300.0]
    assert login_wait_timeouts == [100.0], "the login wait gets only what the port wait left"


@pytest.mark.parametrize("launcher_class", [launcher.LocalProcessLauncher, launcher.DockerComposeLauncher])
@pytest.mark.parametrize("binary_relative", ["", ".", "bin/.."])
def test_launchers_refuse_a_binary_that_names_the_repo_root(tmp_path: Path, launcher_class: type, binary_relative: str) -> None:
    with pytest.raises(ValueError, match="must name a file"):
        launcher_class(repo_root=tmp_path, binary_relative=binary_relative)


def test_require_program_counts_a_directory_as_missing(tmp_path: Path) -> None:
    (tmp_path / "bin").mkdir()
    with pytest.raises(RuntimeError, match="^bin is missing; build it first: "):
        launcher.LocalProcessLauncher(repo_root=tmp_path).require_program("bin")


def test_local_run_to_completion_explains_a_program_in_a_format_this_host_cannot_execute(tmp_path: Path) -> None:
    # No interpreter line and no machine code this host knows: exec fails with ENOEXEC, as an
    # i386 binary does on a Mac.
    program = tmp_path / "bin" / "rotstool"
    program.parent.mkdir()
    program.write_bytes(b"\x7fELF not a program for this host\n")
    program.chmod(0o700)

    with pytest.raises(RuntimeError) as raised:
        launcher.LocalProcessLauncher(repo_root=tmp_path).run_to_completion("bin/rotstool", [], tmp_path, timeout=10)

    assert str(raised.value) == f"cannot execute {program}: this host cannot run the i386 binaries; set ROTS_IT_LAUNCHER=docker"


def test_a_docker_start_whose_port_wait_spent_the_budget_names_the_whole_startup_timeout(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    clock = FakeClock(now=1000.0)
    monkeypatch.setattr(launcher, "time", clock)
    monkeypatch.setattr(launcher.subprocess, "Popen", lambda *args, **kwargs: FakeComposeClient([]))
    monkeypatch.setattr(launcher.subprocess, "run", recording_docker([]))
    monkeypatch.setattr(launcher, "terminate_process", lambda process: None)
    binary = tmp_path / "bin" / "ageland"
    binary.parent.mkdir()
    binary.write_text("", encoding="utf-8")
    run_dir = tmp_path / "run"
    run_dir.mkdir()
    docker = launcher.DockerComposeLauncher(repo_root=tmp_path, startup_timeout=300.0)

    def port_wait_using_the_whole_budget(container_name: str, container_port: int, process: object, log_path: Path, deadline: float) -> int:
        clock.now = deadline
        return 49153

    monkeypatch.setattr(docker, "_wait_for_published_port", port_wait_using_the_whole_budget)

    with pytest.raises(RuntimeError) as raised:
        docker.start(run_dir, run_dir / "lib", port=4321, seed=1)

    assert str(raised.value).startswith(
        f"the server on {launcher.LOOPBACK}:49153 did not send its login prompt within the 300.0s startup timeout, "
        "part of which the wait for the published port used; log tail:"
    )


# Sends a banner at once but the login prompt only after a delay, keeping the connection open.
TEXT_BEFORE_PROMPT_SERVER = FAKE_SERVER_PREAMBLE + """
connection, _address = listener.accept()
connection.sendall(NEGOTIATION + b"Return of the Shadow\\r\\n")
time.sleep(0.5)
(evidence_dir / "prompt-sent").write_text("yes")
connection.sendall(b"Account email: ")
time.sleep(30)
"""


def test_ready_needs_the_prompt_itself_not_just_some_text(tmp_path: Path, fake_servers: FakeServers) -> None:
    port = launcher.allocate_free_port()
    process = fake_servers.start(TEXT_BEFORE_PROMPT_SERVER, port)

    launcher.wait_for_login_prompt(launcher.LOOPBACK, port, 10.0, process, fake_servers.log_path)

    assert (tmp_path / "prompt-sent").exists()
