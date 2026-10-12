from __future__ import annotations

import io
import subprocess
from pathlib import Path

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
    with pytest.raises(RuntimeError, match="cmake --build build --target ageland rotstool"):
        launcher.LocalProcessLauncher(repo_root=tmp_path).require_program("bin/rotstool")


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
    stalling_script.write_text("#!/bin/sh\nsleep 60\n", encoding="utf-8")
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

    stop_calls = [argv for argv in recorded_argv if argv[:4] == ["docker", "stop", "-t", "5"]]
    assert len(stop_calls) == 1
    assert stop_calls[0][4].startswith("rots-it-")
    assert fake_process.terminate_called or fake_process.kill_called
    assert fake_process.stdin.closed


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
