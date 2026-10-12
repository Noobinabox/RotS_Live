"""Runs the repo's i386 programs in one execution environment: the server until stopped, a tool to completion."""

from __future__ import annotations

import os
import shlex
import socket
import subprocess
import sys
import time
import uuid
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator, Optional

LOOPBACK = "127.0.0.1"
INTEGRATION_TMP_RELATIVE = "build/integration/tmp"
BUILD_COMMAND = "make configure && cmake --build build --target ageland rotstool"

# Host variables the local launcher forwards to the server. Everything else is dropped so the
# server sees a clean environment; sanitizer tuning (leak checks, signal handling, symbolizer
# location) is the only legitimate host-to-server channel.
SANITIZER_ENVIRONMENT_VARIABLES = ("ASAN_OPTIONS", "LSAN_OPTIONS", "UBSAN_OPTIONS", "ASAN_SYMBOLIZER_PATH")
# Enough for a whole AddressSanitizer report (header, two stacks, shadow map) when the server
# dies before it listens and the tail is the only evidence left.
LOG_TAIL_BYTES = 16000
# The first prompt the server sends a new connection, once its game loop accepts connections. The
# server listens before it boots the world, so readiness waits for this rather than the port.
LOGIN_EMAIL_PROMPT = "Account email:"
# The pause between two readiness attempts, and the longest one read blocks before the readiness
# wait checks the process again.
READY_RETRY_SECONDS = 0.25


@dataclass
class ServerHandle:
    """A running server: where to connect, where its output goes, and the process to stop."""

    host: str
    port: int
    log_path: Path
    process: subprocess.Popen
    container_name: Optional[str] = None

    def is_alive(self) -> bool:
        """Whether the server process (or the compose run wrapping it) is still running."""
        return self.process.poll() is None

    def exit_status(self) -> int | None:
        """The process's exit status, or None while it runs."""
        return self.process.poll()


@dataclass(frozen=True)
class ToolResult:
    """How a program run to completion ended: its exit status and its combined output."""

    exit_status: int
    output: str


def allocate_free_port() -> int:
    """A loopback TCP port that was free when probed; another process may take it afterwards."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind((LOOPBACK, 0))
        return probe.getsockname()[1]


def read_log_tail(log_path: Path, max_bytes: int = LOG_TAIL_BYTES) -> str:
    """The last max_bytes of the log as text, or an empty string when the log does not exist."""
    if not log_path.exists():
        return ""
    data = log_path.read_bytes()
    return data[-max_bytes:].decode("latin-1", errors="replace")


def check_repo_relative(relative_path: str) -> None:
    """Raises ValueError unless relative_path is relative and stays inside the repository root."""
    normalized = os.path.normpath(relative_path)
    if os.path.isabs(relative_path) or normalized == os.pardir or normalized.startswith(os.pardir + os.sep):
        raise ValueError(f"{relative_path!r} must be a path relative to the repository root that stays inside it")


def wait_for_login_prompt(host: str, port: int, timeout_seconds: float, process: subprocess.Popen, log_path: Path) -> None:
    """Returns once a connection to host:port receives the server's login prompt.

    A refused connection, or one closed before the prompt arrives (as a Docker port proxy does
    while nothing listens behind it), is retried. Raises RuntimeError with the log tail when the
    process exits first or the timeout passes.
    """
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        _raise_if_exited(process, log_path)
        try:
            connection = socket.create_connection((host, port), timeout=1.0)
        except OSError:
            time.sleep(READY_RETRY_SECONDS)
            continue
        with connection:
            if _receives_login_prompt(connection, deadline, process, log_path):
                return
        time.sleep(READY_RETRY_SECONDS)
    _raise_if_exited(process, log_path)
    raise RuntimeError(f"the server on {host}:{port} did not send its login prompt within {timeout_seconds}s; log tail:\n{read_log_tail(log_path)}")


def _raise_if_exited(process: subprocess.Popen, log_path: Path) -> None:
    """Raises RuntimeError with the log tail when the process has exited."""
    if process.poll() is not None:
        raise RuntimeError(f"server exited with status {process.returncode} before it was ready; log tail:\n{read_log_tail(log_path)}")


def _receives_login_prompt(connection: socket.socket, deadline: float, process: subprocess.Popen, log_path: Path) -> bool:
    """Reads until the login prompt arrives (True), or the connection closes or the deadline passes (False)."""
    prompt = LOGIN_EMAIL_PROMPT.encode("ascii")
    connection.settimeout(READY_RETRY_SECONDS)
    received = b""
    while time.monotonic() < deadline:
        _raise_if_exited(process, log_path)
        try:
            chunk = connection.recv(4096)
        except socket.timeout:
            continue
        except OSError:
            return False
        if not chunk:
            return False
        # Telnet negotiation comes before the prompt; a byte search passes over it.
        received += chunk
        if prompt in received:
            return True
        # Keeping one byte less than the prompt still finds a prompt split across two reads.
        received = received[-(len(prompt) - 1):]
    return False


@contextmanager
def _explaining_unexecutable(program_path: Path) -> Iterator[None]:
    """Turns the FileNotFoundError for an existing program this host cannot execute into a RuntimeError saying so."""
    try:
        yield
    except FileNotFoundError as missing:
        # Without the i386 loader, exec fails with ENOENT for a program that exists. The error then
        # names the program; a missing working directory names the directory instead.
        if missing.filename != str(program_path):
            raise
        raise RuntimeError(
            f"cannot execute {program_path}: this host cannot run the i386 binaries; "
            "install the 32-bit runtime (e.g. libc6:i386) or set ROTS_IT_LAUNCHER=docker"
        ) from missing


def close_standard_input(process: subprocess.Popen) -> None:
    """Closes the process's stdin pipe, if it has one; never raises."""
    if process.stdin is None:
        return
    try:
        process.stdin.close()
    except OSError:
        # Closing flushes buffered input; a process that has gone leaves a broken pipe.
        pass


def terminate_process(process: subprocess.Popen) -> None:
    """Terminates the process, escalating to a kill, and closes its stdin pipe.

    Never raises; warns on stderr if the process survives the kill.
    """
    try:
        if process.poll() is not None:
            return
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                # A process the kernel will not reap (stuck in uninterruptible I/O) must not turn
                # fixture teardown into a traceback; the missing exit status reports it instead.
                print(f"server process {process.pid} survived SIGKILL for 10s", file=sys.stderr)
    finally:
        close_standard_input(process)


class ServerLauncher:
    """Runs the repo's programs in one execution environment; each subclass sets _repo_root."""

    # The checkout whose bin/ holds the programs; set by each subclass's constructor.
    _repo_root: Path

    def start(self, run_dir: Path, lib_dir: Path, port: int, seed: int) -> ServerHandle:
        """Starts the server on lib_dir, logging to run_dir/game.log, and returns once it sends its login prompt.

        Raises RuntimeError, after stopping whatever it started, when the server exits or the
        prompt does not arrive within the launcher's startup timeout.
        """
        raise NotImplementedError

    def stop(self, handle: ServerHandle) -> None:
        """Stops the server behind handle and closes its stdin pipe.

        Does not signal a server that has already exited.
        """
        raise NotImplementedError

    def run_to_completion(self, program_relative: str, arguments: list[str], working_dir: Path, timeout: float) -> ToolResult:
        """Runs the repo's program in working_dir and returns its status and combined output.

        Raises subprocess.TimeoutExpired, after stopping the program, when it outlives timeout.
        """
        raise NotImplementedError

    def require_program(self, program_relative: str) -> None:
        """Fails with the build command when the repo's program is missing."""
        if not (self._repo_root / program_relative).exists():
            raise RuntimeError(f"{program_relative} is missing; build it first: {BUILD_COMMAND}")


class LocalProcessLauncher(ServerLauncher):
    """Runs the programs directly on a host that can execute the i386 binaries (Linux, CI)."""

    def __init__(self, repo_root: Path, binary_relative: str = "bin/ageland", startup_timeout: float = 60.0) -> None:
        """Prepares a launcher for the server at repo_root/binary_relative; starts nothing.

        Raises ValueError when binary_relative is absolute or leaves the repo root.
        """
        check_repo_relative(binary_relative)
        # The checkout whose bin/ holds the programs.
        self._repo_root = repo_root
        # The server binary, relative to the repo root.
        self._binary_relative = binary_relative
        # Seconds start waits for the server's login prompt.
        self._startup_timeout = startup_timeout

    def command(self, lib_dir: Path, port: int, seed: int) -> list[str]:
        """The server's argument vector for lib_dir, port and random seed."""
        return [str(self._repo_root / self._binary_relative), "--random-seed", str(seed), "-d", str(lib_dir), str(port)]

    def environment(self) -> dict[str, str]:
        """PATH, HOME and whichever sanitizer variables the host sets; nothing else."""
        server_environment = {"PATH": os.environ.get("PATH", ""), "HOME": os.environ.get("HOME", "")}
        for name in SANITIZER_ENVIRONMENT_VARIABLES:
            if name in os.environ:
                server_environment[name] = os.environ[name]
        return server_environment

    def start(self, run_dir: Path, lib_dir: Path, port: int, seed: int) -> ServerHandle:
        """See ServerLauncher.start; also fails with the build command when the binary is missing.

        Raises RuntimeError naming the i386 runtime when this host cannot execute the binary.
        """
        self.require_program(self._binary_relative)
        log_path = run_dir / "game.log"
        with log_path.open("wb") as log_file, _explaining_unexecutable(self._repo_root / self._binary_relative):
            process = subprocess.Popen(
                self.command(lib_dir, port, seed),
                cwd=run_dir, env=self.environment(), stdin=subprocess.PIPE, stdout=log_file, stderr=subprocess.STDOUT,
            )
        try:
            handle = ServerHandle(LOOPBACK, port, log_path, process)
            wait_for_login_prompt(LOOPBACK, port, self._startup_timeout, process, log_path)
            return handle
        except BaseException:
            # BaseException so a Ctrl-C during the wait also stops the server: the fixture never
            # received the handle, so nothing else would.
            terminate_process(process)
            raise

    def stop(self, handle: ServerHandle) -> None:
        """See ServerLauncher.stop."""
        terminate_process(handle.process)

    def run_to_completion(self, program_relative: str, arguments: list[str], working_dir: Path, timeout: float) -> ToolResult:
        """See ServerLauncher.run_to_completion; also fails with the build command when the program is missing.

        Raises RuntimeError naming the i386 runtime when this host cannot execute the program.
        """
        self.require_program(program_relative)
        program_path = self._repo_root / program_relative
        with _explaining_unexecutable(program_path):
            completed = subprocess.run(
                [str(program_path), *arguments],
                cwd=working_dir, env=self.environment(), stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout,
            )
        return ToolResult(completed.returncode, completed.stdout.decode("latin-1", errors="replace"))


class DockerComposeLauncher(ServerLauncher):
    """Runs the programs through `docker compose run` on hosts that cannot execute i386 binaries (macOS).

    Every container gets a unique name and a random loopback port, so the compose service's fixed
    container name and host port 1024 are never used. Everything a container reads or writes
    must live under the repo root, which the service mounts at /rots.
    """

    def __init__(self, repo_root: Path, binary_relative: str = "bin/ageland", service: str = "rots", startup_timeout: float = 300.0) -> None:
        """Prepares a launcher for the compose service in repo_root; starts nothing.

        Raises ValueError when binary_relative is absolute or leaves the repo root.
        """
        check_repo_relative(binary_relative)
        # The checkout holding docker-compose.yml, mounted at /rots inside the container.
        self._repo_root = repo_root
        # The server binary, relative to the repo root.
        self._binary_relative = binary_relative
        # The compose service every container runs.
        self._service = service
        # Seconds start waits for the port to be published and then for the server's login prompt;
        # generous because the i386 server runs under emulation.
        self._startup_timeout = startup_timeout

    def container_path(self, host_path: Path) -> str:
        """The path the container sees for host_path; the repo root is mounted at /rots."""
        try:
            relative = host_path.resolve().relative_to(self._repo_root.resolve())
        except ValueError as outside:
            raise ValueError(f"{host_path} is outside the repository at {self._repo_root}; the container cannot see it") from outside
        return "/rots/" + relative.as_posix()

    def _run_prefix(self, container_name: str) -> list[str]:
        return [
            "docker", "compose", "run", "--rm", "-T",
            "--name", container_name,
            "-e", f"TMPDIR=/rots/{INTEGRATION_TMP_RELATIVE}",
        ]

    def server_command(self, run_dir: Path, lib_dir: Path, port: int, container_name: str, seed: int) -> list[str]:
        """The compose command that runs the server in run_dir and publishes port on a random loopback port."""
        # ulimit -c 0 keeps a crashing i386 process under QEMU from hanging while it writes a core.
        working_dir = shlex.quote(self.container_path(run_dir))
        binary = shlex.quote(f"/rots/{self._binary_relative}")
        lib = shlex.quote(self.container_path(lib_dir))
        script = f"ulimit -c 0; cd {working_dir} && exec {binary} --random-seed {seed} -d {lib} {port}"
        return [*self._run_prefix(container_name), "-p", f"{LOOPBACK}::{port}", self._service, "bash", "-lc", script]

    def tool_command(self, program_relative: str, arguments: list[str], working_dir: Path, container_name: str) -> list[str]:
        """The compose command that runs the repo's program in working_dir, publishing no port."""
        container_dir = shlex.quote(self.container_path(working_dir))
        program = shlex.quote(f"/rots/{program_relative}")
        script = f"ulimit -c 0; cd {container_dir} && exec {program} {shlex.join(arguments)}"
        return [*self._run_prefix(container_name), self._service, "bash", "-lc", script]

    def _docker_environment(self) -> dict[str, str]:
        # The compose file runs the container as ROTS_UID:ROTS_GID so files it writes under the
        # mount belong to the host user.
        environment = dict(os.environ)
        environment["ROTS_UID"] = str(os.getuid())
        environment["ROTS_GID"] = str(os.getgid())
        return environment

    def _stop_container(self, container_name: str) -> bool:
        """Runs docker stop on the container; True when docker reports it stopped. Never raises."""
        try:
            result = subprocess.run(["docker", "stop", "-t", "5", container_name], capture_output=True, timeout=15)
        except (subprocess.TimeoutExpired, OSError):
            return False
        return result.returncode == 0

    def _remove_container(self, container_name: str) -> None:
        """Force-removes the container, ignoring every failure.

        Compose applies --rm from its client, so a container whose client was killed, or that
        docker stop could not stop, is left behind unless removed here.
        """
        try:
            subprocess.run(["docker", "rm", "-f", container_name], capture_output=True, timeout=15)
        except (subprocess.TimeoutExpired, OSError):
            pass

    def start(self, run_dir: Path, lib_dir: Path, port: int, seed: int) -> ServerHandle:
        """See ServerLauncher.start; also fails with the build command when the binary is missing."""
        (self._repo_root / INTEGRATION_TMP_RELATIVE).mkdir(parents=True, exist_ok=True)
        self.require_program(self._binary_relative)
        container_name = f"rots-it-{uuid.uuid4().hex[:12]}"
        process: Optional[subprocess.Popen] = None
        try:
            log_path = run_dir / "game.log"
            with log_path.open("wb") as log_file:
                process = subprocess.Popen(
                    self.server_command(run_dir, lib_dir, port, container_name, seed),
                    cwd=self._repo_root, env=self._docker_environment(), stdin=subprocess.PIPE, stdout=log_file, stderr=subprocess.STDOUT,
                )
            host_port = self._wait_for_published_port(container_name, port, process, log_path)
            handle = ServerHandle(LOOPBACK, host_port, log_path, process, container_name)
            wait_for_login_prompt(LOOPBACK, host_port, self._startup_timeout, process, log_path)
            return handle
        except BaseException:
            # BaseException so a Ctrl-C during the waits also stops the container: the fixture
            # never received the handle, so nothing else would.
            self._stop_container(container_name)
            self._remove_container(container_name)
            if process is not None:
                terminate_process(process)
            raise

    def _wait_for_published_port(self, container_name: str, container_port: int, process: subprocess.Popen, log_path: Path) -> int:
        """The host port Docker published for container_port; raises RuntimeError if compose exits or the wait times out."""
        deadline = time.monotonic() + self._startup_timeout
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f"docker compose run exited with {process.returncode}; log tail:\n{read_log_tail(log_path)}")
            try:
                result = subprocess.run(["docker", "port", container_name, f"{container_port}/tcp"], capture_output=True, text=True, timeout=10)
            except subprocess.TimeoutExpired:
                time.sleep(1.0)
                continue
            if result.returncode == 0 and ":" in result.stdout:
                return int(result.stdout.strip().rsplit(":", 1)[1])
            time.sleep(1.0)
        raise RuntimeError(f"container {container_name} never published port {container_port}; log tail:\n{read_log_tail(log_path)}")

    def stop(self, handle: ServerHandle) -> None:
        """See ServerLauncher.stop; stops the container, then waits for (or kills) the compose run.

        Removes the container itself when docker stop fails or the compose run has to be killed.
        """
        if handle.container_name and not self._stop_container(handle.container_name):
            self._remove_container(handle.container_name)
        if handle.is_alive():
            try:
                handle.process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                handle.process.kill()
                try:
                    handle.process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    print(f"docker compose run process {handle.process.pid} survived SIGKILL for 10s", file=sys.stderr)
                if handle.container_name:
                    self._remove_container(handle.container_name)
        close_standard_input(handle.process)

    def run_to_completion(self, program_relative: str, arguments: list[str], working_dir: Path, timeout: float) -> ToolResult:
        """See ServerLauncher.run_to_completion; also fails with the build command when the program is missing."""
        self.require_program(program_relative)
        (self._repo_root / INTEGRATION_TMP_RELATIVE).mkdir(parents=True, exist_ok=True)
        container_name = f"rots-it-tool-{uuid.uuid4().hex[:12]}"
        try:
            completed = subprocess.run(
                self.tool_command(program_relative, arguments, working_dir, container_name),
                cwd=self._repo_root, env=self._docker_environment(), stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout,
            )
        except BaseException:
            # A timeout or a Ctrl-C kills only the compose client; the container keeps running
            # until stopped here, and stays until removed here because its --rm died with the client.
            self._stop_container(container_name)
            self._remove_container(container_name)
            raise
        return ToolResult(completed.returncode, completed.stdout.decode("latin-1", errors="replace"))
