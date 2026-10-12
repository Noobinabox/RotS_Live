from __future__ import annotations

from pathlib import Path

import pytest

from rots_harness import dockerlock


class FakeClock:
    def __init__(self) -> None:
        # Seconds since the test started; advances only when sleep is called.
        self.now = 0.0

    def __call__(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


def test_docker_lock_refuses_while_another_lock_exists(tmp_path: Path) -> None:
    lock_dir = tmp_path / "locks"
    lock_dir.mkdir()
    (lock_dir / "someone-else.lock").write_text("session: other\n", encoding="utf-8")
    lock = dockerlock.DockerLock(lock_dir, repo_root=tmp_path, wait_seconds=0)
    with pytest.raises(dockerlock.DockerLockHeld, match="someone-else.lock"):
        lock.acquire(purpose="unit test")
    assert lock.path is None
    assert sorted(path.name for path in lock_dir.iterdir()) == ["someone-else.lock"]


def test_docker_lock_writes_a_uniquely_named_file_and_removes_it_on_release(tmp_path: Path) -> None:
    lock_dir = tmp_path / "locks"
    lock_dir.mkdir()
    lock = dockerlock.DockerLock(lock_dir, repo_root=tmp_path, wait_seconds=0)
    lock.acquire(purpose="unit test")
    lock_path = lock.path
    assert lock_path is not None and lock_path.is_file()
    assert lock_path.parent == lock_dir
    assert lock_path.name.startswith(f"{dockerlock.LOCK_FILE_PREFIX}-") and lock_path.name.endswith(".lock")
    body_lines = lock_path.read_text(encoding="utf-8").splitlines()
    assert "purpose: unit test" in body_lines
    assert f"repo: {tmp_path}" in body_lines
    assert "service: rots" in body_lines
    assert "duration: pytest session" in body_lines
    lock.release()
    assert not lock_path.exists()
    assert lock.path is None


def test_second_docker_lock_refuses_while_the_first_is_held(tmp_path: Path) -> None:
    lock_dir = tmp_path / "locks"
    lock_dir.mkdir()
    first = dockerlock.DockerLock(lock_dir, repo_root=tmp_path, wait_seconds=0)
    first.acquire(purpose="first session")
    assert first.path is not None
    second = dockerlock.DockerLock(lock_dir, repo_root=tmp_path, wait_seconds=0)
    with pytest.raises(dockerlock.DockerLockHeld, match=first.path.name):
        second.acquire(purpose="second session")
    assert second.path is None
    first.release()


def test_docker_lock_fails_when_the_directory_is_missing(tmp_path: Path) -> None:
    lock = dockerlock.DockerLock(tmp_path / "missing", repo_root=tmp_path, wait_seconds=0)
    with pytest.raises(RuntimeError, match="is missing"):
        lock.acquire(purpose="unit test")
    assert not (tmp_path / "missing").exists()


def test_docker_lock_backs_off_when_another_lock_appears_after_its_own(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    lock_dir = tmp_path / "locks"
    lock_dir.mkdir()
    created_paths: list[Path] = []
    real_open = Path.open

    def open_then_plant_rival(self: Path, mode: str = "r", *args: object, **kwargs: object):
        handle = real_open(self, mode, *args, **kwargs)
        if mode == "x":
            created_paths.append(self)
            (lock_dir / "rival-session.lock").write_text("session: rival\n", encoding="utf-8")
        return handle

    monkeypatch.setattr(Path, "open", open_then_plant_rival)
    lock = dockerlock.DockerLock(lock_dir, repo_root=tmp_path, wait_seconds=0)
    with pytest.raises(dockerlock.DockerLockHeld, match="rival-session.lock"):
        lock.acquire(purpose="unit test")
    assert len(created_paths) == 1
    assert not created_paths[0].exists()
    assert lock.path is None
    assert sorted(path.name for path in lock_dir.iterdir()) == ["rival-session.lock"]


def test_docker_lock_without_a_directory_does_nothing(tmp_path: Path) -> None:
    lock = dockerlock.DockerLock(None, repo_root=tmp_path)
    lock.acquire(purpose="unit test")
    assert lock.path is None
    lock.release()


def test_the_lock_waits_for_the_holder_and_then_takes_it(tmp_path: Path) -> None:
    lock_dir = tmp_path / "locks"
    lock_dir.mkdir()
    holder = lock_dir / "other-session.lock"
    holder.write_text("session: other\n", encoding="utf-8")
    clock = FakeClock()
    reports: list[str] = []

    def sleep_then_release(seconds: float) -> None:
        clock.sleep(seconds)
        if clock.now >= 10:
            holder.unlink(missing_ok=True)

    lock = dockerlock.DockerLock(lock_dir, repo_root=tmp_path, wait_seconds=60, poll_seconds=5, clock=clock, sleep=sleep_then_release, report=reports.append)
    lock.acquire(purpose="unit test")
    assert lock.path is not None and lock.path.exists()
    assert clock.now == 10
    assert len(reports) == 1 and "other-session.lock" in reports[0]
    lock.release()


def test_the_lock_gives_up_after_the_wait_and_names_the_holder(tmp_path: Path) -> None:
    lock_dir = tmp_path / "locks"
    lock_dir.mkdir()
    (lock_dir / "other-session.lock").write_text("session: other\n", encoding="utf-8")
    clock = FakeClock()
    lock = dockerlock.DockerLock(lock_dir, repo_root=tmp_path, wait_seconds=30, poll_seconds=5, clock=clock, sleep=clock.sleep, report=lambda _message: None)
    with pytest.raises(dockerlock.DockerLockHeld, match="other-session.lock"):
        lock.acquire(purpose="unit test")
    assert clock.now >= 30
    assert sorted(path.name for path in lock_dir.iterdir()) == ["other-session.lock"]
