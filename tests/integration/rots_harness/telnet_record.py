"""Records a session's telnet events in order, logs each one, and lets waits claim received events."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Callable, Optional

from rots_harness.telnet import TelnetEvent, TelnetText, describe_event


class TelnetDirection(Enum):
    """Whether the session read an event from the server or wrote it to the server."""

    # Read from the server.
    RECEIVED = "received"
    # Written to the server.
    SENT = "sent"


@dataclass(frozen=True)
class RecordedTelnetEvent:
    """One event with its place in the session's record and its direction."""

    # Position in the record, counting from 1.
    sequence: int
    # Whether the event was read or written.
    direction: TelnetDirection
    # The event itself.
    event: TelnetEvent


class TelnetRecord:
    """Keeps one session's telnet events in order with their direction, mirrors each to the session's telnet log, and hands each matching event to at most one wait."""

    def __init__(self, log_path: Path) -> None:
        # The telnet log; created by the first add() and appended to by each one.
        self._log_path = log_path
        # Every event added, oldest first.
        self._events: list[RecordedTelnetEvent] = []
        # Sequence numbers of the events a wait has claimed.
        self._claimed: set[int] = set()

    def add(self, direction: TelnetDirection, event: TelnetEvent) -> RecordedTelnetEvent:
        """Records an event and appends its log line at once, so a crashed run keeps the log up to the crash."""
        recorded = RecordedTelnetEvent(len(self._events) + 1, direction, event)
        self._events.append(recorded)
        with self._log_path.open("a", encoding="utf-8", newline="") as log_file:
            log_file.write(f"{recorded.sequence:06d} {direction.value:<8} {describe_event(event)}\n")
        return recorded

    @property
    def events(self) -> tuple[RecordedTelnetEvent, ...]:
        """Every event added, oldest first."""
        return tuple(self._events)

    def claim_first(self, matches: Callable[[RecordedTelnetEvent], bool]) -> Optional[RecordedTelnetEvent]:
        """The oldest received, unclaimed event that matches, now claimed; None if there is none."""
        for recorded in self._events:
            if recorded.direction is not TelnetDirection.RECEIVED or recorded.sequence in self._claimed:
                continue
            if matches(recorded):
                self._claimed.add(recorded.sequence)
                return recorded
        return None

    def unclaimed_received(self) -> list[RecordedTelnetEvent]:
        """Received events other than text that no wait has claimed, oldest first.

        Text is left out because no wait claims it, and it would crowd out the negotiation a timeout message is meant to show.
        """
        return [
            recorded
            for recorded in self._events
            if recorded.direction is TelnetDirection.RECEIVED
            and recorded.sequence not in self._claimed
            and not isinstance(recorded.event, TelnetText)
        ]
