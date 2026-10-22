"""One character's telnet connection to the game: logs in, sends commands, negotiates as a client profile directs, records what the player reads and the wire, waits for telnet events, and ends a test's connections."""

from __future__ import annotations

import re
import socket
import time
from pathlib import Path
from typing import Callable, Iterable, List, Optional, Sequence

from rots_harness.launcher import LOGIN_EMAIL_PROMPT, ServerHandle
from rots_harness.telnet import (
    OPTION_MCCP2,
    OPTION_MSDP,
    OPTION_NAMES,
    TelnetNegotiation,
    TelnetStreamParser,
    TelnetSubnegotiation,
    TelnetText,
    describe_event,
    encode_event,
    escape_iac,
    negotiation_bytes,
    subnegotiation_bytes,
)
from rots_harness.telnet_negotiator import TelnetNegotiator
from rots_harness.telnet_payloads import TelnetPayloadError, encode_msdp
from rots_harness.telnet_profile import SILENT_CLIENT, TelnetClientProfile
from rots_harness.telnet_record import RecordedTelnetEvent, TelnetDirection, TelnetRecord

LOGIN_PASSWORD_PROMPT = "Account password:"
ACCOUNT_MENU_PROMPT = "Choice:"
ACCOUNT_MENU_PLAY = "2"
CHARACTER_CHOICE_PROMPT = "Character number or name:"
CHARACTER_MENU_PROMPT = "Make your choice:"
CHARACTER_MENU_ENTER = "1"
ENTER_GAME_MARKER = "Here we go..."
BAD_CREDENTIALS_MARKER = "Invalid account credentials"
ONE_CHARACTER_ONLINE_MARKER = "You are already connected as"
ONE_CHARACTER_ONLINE_RULE = "an account may have one character online unless it owns a character above level 91 (interpre.cpp, first_restricting_active_account_session_for_account)"
PROMPT_TERMINATORS = (">", "]")
# A complete control sequence: colour codes and the clear-screen pair ESC[H ESC[J.
CSI_PATTERN = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
# A control sequence cut off at the end of the received text, still waiting for its final letter.
INCOMPLETE_CSI_PATTERN = re.compile(r"\x1b(\[[0-9;?]*)?\Z")
# How much of the unread text an error message quotes.
PENDING_TEXT_QUOTED = 1500
# How many of the most recent unclaimed telnet events a wait-timeout message lists.
UNCLAIMED_EVENTS_LISTED = 40
FAREWELL_MARKERS = ("Goodbye", "As you quit")


class SessionTimeout(AssertionError):
    """The server did not send an awaited marker or prompt in time; the message names it and quotes the pending text."""


class QuitRefused(RuntimeError):
    """The server did not answer `quit` with a farewell, so the character may still be in the game."""


class LoginRefused(AssertionError):
    """The game refused the login; the message carries the game's words."""


class TelnetCompressionUnsupported(ConnectionError):
    """The server started MCCP2 compression, which the harness cannot read; RotS never compresses, so the server has changed.

    The session is closed when this is raised.
    """


def ends_with_prompt(text: str) -> bool:
    """Whether text, ignoring trailing whitespace, ends in a game prompt."""
    stripped = text.rstrip()
    return bool(stripped) and stripped.endswith(PROMPT_TERMINATORS)


class Transcript:
    """The text one command produced, with lookups for the lines scenarios read."""

    def __init__(self, text: str) -> None:
        # The sanitized, colour-free text the command produced.
        self.text = text

    def contains(self, marker: str) -> bool:
        """Whether marker appears anywhere in the text."""
        return marker in self.text

    def room_name(self) -> Optional[str]:
        """The first non-empty line without its exits suffix, or None when the text is blank."""
        for line in self.text.splitlines():
            if line.strip():
                # Two or more spaces separate a room's name from its "Exits are:" list.
                return re.split(r"\s{2,}", line.strip(), maxsplit=1)[0]
        return None


class GameSession:
    """One character's connection to a running server: wires the telnet parser, negotiator and record into its read and send paths and exposes waits for telnet events.

    Protocol decisions belong to the negotiator and event bookkeeping to the record. The text the
    character reads, the raw bytes both ways and every telnet event are mirrored to files in the
    transcript directory as they pass.
    """

    def __init__(self, handle: ServerHandle, character_name: str, email: str, password: str, transcript_dir: Path, client: TelnetClientProfile = SILENT_CLIENT) -> None:
        """Connects to the server and answers its negotiation as client directs; login() then brings the character into the game, picking it from the account's list by name."""
        # Names the character in errors and picks it at the character list; the session's file names use its lower-case form.
        self._character_name = character_name
        # The account credentials login() sends.
        self._email = email
        self._password = password
        # The profile the negotiator answers the server's negotiation with.
        self._client = client
        # The game connection; owned by this session and released by close().
        self._socket = socket.create_connection((handle.host, handle.port), timeout=5.0)
        # A short read timeout lets the wait loops check their deadlines between reads.
        self._socket.settimeout(0.25)
        # Splits received bytes into text and telnet events, carrying a sequence split across reads.
        self._received_parser = TelnetStreamParser()
        # Splits sent bytes into events for the record; separate because each direction is its own stream.
        self._sent_parser = TelnetStreamParser()
        # Decides the replies to the server's negotiation.
        self._negotiator = TelnetNegotiator(client)
        # Every raw byte received so far; only ever appended to.
        self._received_bytes = bytearray()
        # Everything received so far, without telnet negotiation or control sequences; only ever appended to.
        self._clean = ""
        # Everything received so far without telnet negotiation, colour and other control sequences kept; only ever appended to.
        self._raw = ""
        # The start of a control sequence split across reads, held back until its final letter arrives.
        self._held_escape = ""
        # Length of the prefix of _clean already returned by expect, drain or command.
        self._consumed = 0
        # Set once close() runs; the socket is gone from then on.
        self._closed = False
        # Set once login() has brought the character into the game.
        self._logged_in = False
        transcript_dir.mkdir(parents=True, exist_ok=True)
        file_stem = character_name.lower()
        # Appended to after every read so a failed run keeps what the character saw.
        self._transcript_path = transcript_dir / f"{file_stem}.txt"
        # The raw bytes each way, appended as they arrive or leave so a failed run keeps the wire.
        self._received_path = transcript_dir / f"{file_stem}.received.bin"
        self._sent_path = transcript_dir / f"{file_stem}.sent.bin"
        # Every telnet event both ways, text included, logged one line each to <name>.telnet.log.
        self._record = TelnetRecord(transcript_dir / f"{file_stem}.telnet.log")

    @property
    def character_name(self) -> str:
        """The name of the character this session plays."""
        return self._character_name

    @property
    def client(self) -> TelnetClientProfile:
        """The profile this session answers the server's negotiation with."""
        return self._client

    @property
    def everything(self) -> str:
        """All text received so far, without telnet negotiation or colour codes."""
        return self._clean

    @property
    def everything_raw(self) -> str:
        """All text received so far, without telnet negotiation but with colour and other control sequences kept."""
        return self._raw

    @property
    def received_bytes(self) -> bytes:
        """Every byte received so far, exactly as it arrived."""
        return bytes(self._received_bytes)

    @property
    def telnet_events(self) -> tuple[RecordedTelnetEvent, ...]:
        """Every telnet event received or sent so far, text included, oldest first."""
        return self._record.events

    @property
    def logged_in(self) -> bool:
        """True once login() has brought the character into the game."""
        return self._logged_in

    @property
    def is_closed(self) -> bool:
        """True once the session has closed its connection."""
        return self._closed

    def _pump(self) -> bool:
        """Reads one chunk if any arrives within the read timeout, records its events and sends the negotiator's replies; returns whether one arrived.

        Raises ConnectionError when the server closes the connection, TelnetCompressionUnsupported when it
        starts MCCP2, and TelnetPayloadError for a malformed CHARSET request once the whole chunk is recorded.
        """
        try:
            chunk = self._socket.recv(4096)
        except socket.timeout:
            return False
        if not chunk:
            raise ConnectionError(f"{self.character_name}: server closed the connection; unread text:\n{self._unconsumed()[-PENDING_TEXT_QUOTED:]}")
        self._received_bytes.extend(chunk)
        with self._received_path.open("ab") as received_file:
            received_file.write(chunk)
        text_parts: list[bytes] = []
        compression_started = False
        # The first reply failure in this chunk; raised once every event of the chunk is recorded.
        reply_error: Optional[TelnetPayloadError] = None
        try:
            for event in self._received_parser.feed(chunk):
                self._record.add(TelnetDirection.RECEIVED, event)
                if isinstance(event, TelnetText):
                    text_parts.append(event.data)
                elif isinstance(event, TelnetSubnegotiation) and event.option == OPTION_MCCP2:
                    # Every byte after this frame is compressed, so the rest of the chunk is not telnet.
                    compression_started = True
                    break
                try:
                    replies = self._negotiator.respond(event)
                except TelnetPayloadError as error:
                    # The events after a malformed request still belong in the record and the text.
                    if reply_error is None:
                        reply_error = error
                    continue
                for reply in replies:
                    self._send(encode_event(reply))
        finally:
            # Text read before a failure still reaches everything and the transcript.
            self._add_text(b"".join(text_parts).decode("latin-1"))
        if compression_started:
            # A later read would parse compressed bytes as telnet, and a quit could only time out.
            self.close()
            raise TelnetCompressionUnsupported(f"{self.character_name}: the server started MCCP2 compression, which the harness cannot read; unread text:\n{self._unconsumed()[-PENDING_TEXT_QUOTED:]}")
        if reply_error is not None:
            raise reply_error
        return True

    def _add_text(self, text: str) -> None:
        """Appends one read's text to everything_raw, and with control sequences stripped to everything and the transcript."""
        self._raw += text
        received = self._held_escape + text
        incomplete = INCOMPLETE_CSI_PATTERN.search(received)
        if incomplete is None:
            self._held_escape = ""
        else:
            self._held_escape = received[incomplete.start():]
            received = received[:incomplete.start()]
        fresh = CSI_PATTERN.sub("", received)
        self._clean += fresh
        with self._transcript_path.open("a", encoding="utf-8", newline="") as transcript_file:
            transcript_file.write(fresh)

    def _unconsumed(self) -> str:
        return self._clean[self._consumed:]

    def _consume(self) -> str:
        text = self._unconsumed()
        self._consumed = len(self._clean)
        return text

    def expect(self, markers: Sequence[str], timeout: float = 8.0) -> str:
        """Waits until any marker appears in the unread text, then returns all of it as read; raises SessionTimeout."""
        deadline = time.monotonic() + timeout
        while True:
            pending = self._unconsumed()
            if any(marker in pending for marker in markers):
                return self._consume()
            if time.monotonic() >= deadline:
                raise SessionTimeout(f"{self.character_name}: none of {list(markers)!r} within {timeout}s; pending text:\n{pending[-PENDING_TEXT_QUOTED:]}")
            self._pump()

    def drain(self, timeout: float = 0.5) -> str:
        """Reads for timeout seconds and returns all unread text as read."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self._pump()
        return self._consume()

    def expect_negotiation(self, command: int, option: int, timeout: float = 8.0) -> RecordedTelnetEvent:
        """The oldest received, unclaimed IAC command option (WILL, WONT, DO or DONT), now claimed; reads until it arrives, or raises SessionTimeout.

        Events that arrived before the call count, and no wait returns an event twice. Text is left unread.
        """

        def matches(recorded: RecordedTelnetEvent) -> bool:
            return recorded.event == TelnetNegotiation(command, option)

        return self._expect_event(matches, describe_event(TelnetNegotiation(command, option)), timeout)

    def expect_subnegotiation(self, option: int, timeout: float = 8.0, matching: Optional[Callable[[bytes], bool]] = None) -> RecordedTelnetEvent:
        """The oldest received, unclaimed subnegotiation for option whose payload passes matching (any payload when None), now claimed; raises SessionTimeout.

        Events that arrived before the call count, and no wait returns an event twice. Text is left unread.
        """

        def matches(recorded: RecordedTelnetEvent) -> bool:
            event = recorded.event
            return isinstance(event, TelnetSubnegotiation) and event.option == option and (matching is None or matching(event.payload))

        description = f"SB {OPTION_NAMES.get(option, str(option))}"
        if matching is not None:
            description += " with a matching payload"
        return self._expect_event(matches, description, timeout)

    def _expect_event(self, matches: Callable[[RecordedTelnetEvent], bool], description: str, timeout: float) -> RecordedTelnetEvent:
        """Claims the first received event that matches, reading until one arrives; the timeout lists the events no wait has claimed."""
        deadline = time.monotonic() + timeout
        while True:
            claimed = self._record.claim_first(matches)
            if claimed is not None:
                return claimed
            if time.monotonic() >= deadline:
                unclaimed = self._record.unclaimed_received()[-UNCLAIMED_EVENTS_LISTED:]
                listing = "\n".join(describe_event(recorded.event) for recorded in unclaimed) or "(none)"
                raise SessionTimeout(f"{self.character_name}: no {description} within {timeout}s; unclaimed telnet events received:\n{listing}")
            self._pump()

    def _send(self, data: bytes) -> None:
        """The only path to the server: sends data, then appends it to <name>.sent.bin and records its events as sent."""
        self._socket.sendall(data)
        with self._sent_path.open("ab") as sent_file:
            sent_file.write(data)
        for event in self._sent_parser.feed(data):
            self._record.add(TelnetDirection.SENT, event)

    def send_line(self, line: str) -> None:
        """Sends line followed by a newline; characters outside Latin-1 become '?' and a 0xFF is doubled so it stays text."""
        self._send(escape_iac(line.encode("latin-1", errors="replace")) + b"\n")

    def send_bytes(self, data: bytes) -> None:
        """Sends data exactly as given, with no escaping; the negotiator does not learn of any telnet command in it."""
        self._send(data)

    def send_negotiation(self, command: int, option: int) -> None:
        """Sends IAC command option; the negotiator does not learn of it, so it answers the server's reply as an unprompted one."""
        self._send(negotiation_bytes(command, option))

    def send_subnegotiation(self, option: int, payload: bytes) -> None:
        """Sends IAC SB option payload IAC SE, with 0xFF in the payload doubled."""
        self._send(subnegotiation_bytes(option, payload))

    def send_msdp(self, name: str, *values: str) -> None:
        """Sends an MSDP frame setting variable name to values; raises ValueError for an empty name, no values or MSDP marker bytes."""
        self.send_subnegotiation(OPTION_MSDP, encode_msdp(name, *values))

    def command(self, text: str, timeout: float = 8.0) -> Transcript:
        """Discards unread text, sends text and returns its output up to the next prompt; raises SessionTimeout."""
        self.drain(0.1)
        self.send_line(text)
        deadline = time.monotonic() + timeout
        while True:
            pending = self._unconsumed()
            if ends_with_prompt(pending):
                return Transcript(self._consume())
            if time.monotonic() >= deadline:
                raise SessionTimeout(f"{self.character_name}: no prompt after {text!r} within {timeout}s; pending text:\n{pending[-PENDING_TEXT_QUOTED:]}")
            self._pump()

    def login(self, timeout: float = 15.0) -> None:
        """Logs in through the account menu, picks the character by name and enters the game; raises LoginRefused with the game's words."""
        self.expect([LOGIN_EMAIL_PROMPT], timeout)
        self.send_line(self._email)
        self.expect([LOGIN_PASSWORD_PROMPT], timeout)
        self.send_line(self._password)
        self._expect_or_refuse([ACCOUNT_MENU_PROMPT], timeout)
        self.send_line(ACCOUNT_MENU_PLAY)
        self._expect_or_refuse([CHARACTER_CHOICE_PROMPT], timeout)
        # The server matches the name against the whole roster ignoring case, so menu order does not matter.
        self.send_line(self._character_name)
        self._expect_or_refuse([CHARACTER_MENU_PROMPT], timeout)
        self.send_line(CHARACTER_MENU_ENTER)
        self._expect_or_refuse([ENTER_GAME_MARKER], timeout)
        self.command("look", timeout)
        self._logged_in = True

    def _expect_or_refuse(self, markers: List[str], timeout: float) -> str:
        """Like expect, but raises LoginRefused as soon as the game refuses the login instead of waiting out the timeout."""
        text = self.expect([*markers, BAD_CREDENTIALS_MARKER, ONE_CHARACTER_ONLINE_MARKER], timeout)
        if BAD_CREDENTIALS_MARKER in text:
            raise LoginRefused(f"{self.character_name}: the server refused the account login: {BAD_CREDENTIALS_MARKER}")
        if ONE_CHARACTER_ONLINE_MARKER in text:
            refusal = next(line.strip() for line in text.splitlines() if ONE_CHARACTER_ONLINE_MARKER in line)
            raise LoginRefused(f"{self.character_name}: {refusal} The game's rule: {ONE_CHARACTER_ONLINE_RULE}.")
        return text

    def quit(self, timeout: float = 8.0) -> None:
        """Quits the game and closes the connection either way; raises QuitRefused when no farewell arrives.

        Text received before the quit is discarded first, so only a farewell that answers this quit counts.
        """
        try:
            self.drain(0.1)
            self.send_line("quit")
            self.expect(FAREWELL_MARKERS, timeout)
        except SessionTimeout as refusal:
            raise QuitRefused(f"{self.character_name}: quit was refused or never answered") from refusal
        finally:
            # The game returns a quitting character to the character menu with the socket
            # still open; closing it is what ends the session.
            self.close()

    def close(self) -> None:
        """Closes the connection; later calls do nothing more."""
        self._closed = True
        try:
            self._socket.close()
        except OSError:
            pass


def end_sessions(sessions: Iterable[GameSession], server_alive: bool) -> list[str]:
    """Quits each open session that logged in, or only closes it when it never logged in or the server has exited; returns the quit failures' messages.

    Sessions already closed are skipped. A failed quit does not stop the rest from ending.
    """
    problems: list[str] = []
    for game_session in sessions:
        if game_session.is_closed:
            continue
        if not server_alive or not game_session.logged_in:
            # An exited server cannot answer quit, and the caller's crash checks report the exit;
            # a quit attempt would only add a second, misleading failure. A session that never
            # reached the game has no character to quit, so closing it is the whole ending.
            game_session.close()
            continue
        try:
            game_session.quit()
        except (QuitRefused, ConnectionError) as quit_failure:
            problems.append(str(quit_failure))
    return problems
