"""Telnet clients: each harness client profile negotiates with the real server as the server's protocol code says it should."""

from __future__ import annotations

from typing import Iterable

import pytest

from rots_harness import world
from rots_harness.characters import custom_mage
from rots_harness.launcher import LOGIN_EMAIL_PROMPT
from rots_harness.session import GameSession
from rots_harness.telnet import (
    DO,
    DONT,
    OPTION_ATCP,
    OPTION_CHARSET,
    OPTION_ECHO,
    OPTION_MSDP,
    OPTION_MSP,
    OPTION_MSSP,
    OPTION_MXP,
    OPTION_NAWS,
    OPTION_TTYPE,
    WILL,
    WONT,
    TelnetEvent,
    TelnetNegotiation,
    TelnetSubnegotiation,
    TelnetText,
    describe_event,
)
from rots_harness.telnet_payloads import MSDP_VAL, MSDP_VAR, TTYPE_SEND, decode_msdp, decode_mssp
from rots_harness.telnet_profile import FULL_CLIENT, MSDP_CLIENT, REFUSING_CLIENT
from rots_harness.telnet_record import TelnetDirection

pytestmark = [pytest.mark.scenario, pytest.mark.telnet]

# A mage with a custom profession split, logging in to the arena.
MAGE = custom_mage(level=30, load_room=world.ROOM_ARENA_CENTRE)

# What the server sends a new connection before the email prompt: a request for the terminal type.
CONNECT_OFFER = TelnetNegotiation(DO, OPTION_TTYPE)

# The server's first eight offers, in order: DO TTYPE at connect, then the seven it makes once the
# client answers DO TTYPE either way.
OFFERS_IN_ORDER = [
    TelnetNegotiation(DO, OPTION_TTYPE),
    TelnetNegotiation(DO, OPTION_NAWS),
    TelnetNegotiation(DO, OPTION_CHARSET),
    TelnetNegotiation(WILL, OPTION_MSDP),
    TelnetNegotiation(WILL, OPTION_MSSP),
    TelnetNegotiation(DO, OPTION_ATCP),
    TelnetNegotiation(WILL, OPTION_MSP),
    TelnetNegotiation(DO, OPTION_MXP),
]

# The commands that offer or request an option; the server's offers use only these.
OFFER_COMMANDS = (WILL, DO)

# The commands that refuse an option.
REFUSAL_COMMANDS = (WONT, DONT)

# SB TTYPE SEND: the server asking for the next terminal type.
TTYPE_SEND_PAYLOAD = bytes([TTYPE_SEND])

# How many times the server asks MSDP_CLIENT for a terminal type: it stops once a reply repeats
# the one before.
MSDP_CLIENT_TTYPE_REQUESTS = 4

# MSDP_CLIENT's TTYPE IS replies, one per request; the last name repeats.
MSDP_CLIENT_TTYPE_REPLIES = [b"\x00ROTS-HARNESS", b"\x00XTERM-256COLOR", b"\x00MTTS 9", b"\x00MTTS 9"]

# MSDP_CLIENT's CHARSET reply: ACCEPTED, then the one character set the server offers.
MSDP_CLIENT_CHARSET_REPLIES = [b"\x02ISO-8859-1"]

# The variable an MSDP LIST COMMANDS reply sets.
MSDP_COMMANDS_VARIABLE = "COMMANDS"

# The start of the MSDP frame that sets COMMANDS; selecting by it leaves other frames undecoded.
COMMANDS_FRAME_START = bytes([MSDP_VAR]) + MSDP_COMMANDS_VARIABLE.encode("ascii") + bytes([MSDP_VAL])

# The server's answer to MSDP LIST COMMANDS.
MSDP_COMMANDS_REPLY = {MSDP_COMMANDS_VARIABLE: ["LIST", "REPORT", "RESET", "SEND", "UNREPORT"]}

# The command that turns the msdp preference on; the server sends in-game MSDP frames only with it.
MSDP_ON_COMMAND = "set msdp on"

# The game's reply when the msdp preference turns on.
MSDP_ON_REPLY = "Msdp mode is now on."

# The variable carrying the vnum of the character's room.
ROOM_VNUM_VARIABLE = "ROOM_VNUM"

# The start of the MSDP frame that sets ROOM_VNUM; the server sends one variable per frame.
ROOM_VNUM_FRAME_START = bytes([MSDP_VAR]) + ROOM_VNUM_VARIABLE.encode("ascii") + bytes([MSDP_VAL])

# The MSSP status the server sends once a client accepts MSSP.
SERVER_STATUS = {"NAME": ["Return of the Shadow"], "PLAYERS": ["0"], "UPTIME": ["0"], "CRAWL DELAY": ["-1"]}

# The control sequence the server sends as text after its empty SB MXP, opening a secure MXP channel.
MXP_SECURE_CHANNEL = "\x1b[7z"


def events_in_direction(game_session: GameSession, direction: TelnetDirection) -> list[TelnetEvent]:
    """The session's telnet events in one direction, text included, oldest first."""
    return [recorded.event for recorded in game_session.telnet_events if recorded.direction is direction]


def non_text_received_before_email_prompt(game_session: GameSession) -> list[TelnetEvent]:
    """The received events other than text that arrived before the text completing the email prompt.

    The prompt is found in the text accumulated across text events, since a read can split it.
    Fails the test when the prompt never arrived.
    """
    prompt = LOGIN_EMAIL_PROMPT.encode("latin-1")
    received_text = bytearray()
    non_text: list[TelnetEvent] = []
    for event in events_in_direction(game_session, TelnetDirection.RECEIVED):
        if isinstance(event, TelnetText):
            received_text.extend(event.data)
            if prompt in received_text:
                return non_text
        else:
            non_text.append(event)
    pytest.fail(f"{game_session.character_name}: no {LOGIN_EMAIL_PROMPT!r} among the received text events")


def subnegotiation_payloads(events: Iterable[TelnetEvent], option: int) -> list[bytes]:
    """The payloads of the subnegotiations for option, in order."""
    return [event.payload for event in events if isinstance(event, TelnetSubnegotiation) and event.option == option]


def wire_listing(game_session: GameSession) -> str:
    """Every telnet event other than text, one line each with its direction, for an assertion message."""
    return "\n".join(
        f"{recorded.direction.value:<8} {describe_event(recorded.event)}"
        for recorded in game_session.telnet_events
        if not isinstance(recorded.event, TelnetText)
    )


def sets_msdp_commands(payload: bytes) -> bool:
    """Whether an MSDP frame sets the COMMANDS variable."""
    return payload.startswith(COMMANDS_FRAME_START)


def sets_room_vnum(payload: bytes) -> bool:
    """Whether an MSDP frame sets the ROOM_VNUM variable."""
    return payload.startswith(ROOM_VNUM_FRAME_START)


def is_empty(payload: bytes) -> bool:
    """Whether a subnegotiation carries no payload."""
    return payload == b""


@pytest.mark.characters(MAGE)
def test_a_silent_client_is_offered_only_ttype(server) -> None:
    mage = server.connect(MAGE.name)
    mage.login()
    # The password prompt's WILL ECHO comes after the email prompt and is not an offer.
    assert non_text_received_before_email_prompt(mage) == [CONNECT_OFFER], wire_listing(mage)
    sent = events_in_direction(mage, TelnetDirection.SENT)
    assert all(isinstance(event, TelnetText) for event in sent), wire_listing(mage)


@pytest.mark.characters(MAGE)
def test_a_refusing_client_is_offered_every_other_option_and_refuses_each(server) -> None:
    mage = server.connect(MAGE.name, client=REFUSING_CLIENT)
    mage.expect_negotiation(DO, OPTION_MXP)
    mage.login()
    received = events_in_direction(mage, TelnetDirection.RECEIVED)
    offers = [event for event in received if isinstance(event, TelnetNegotiation) and event.command in OFFER_COMMANDS]
    # Later offers are the server's WILL MXP after our WONT MXP and its DO TTYPE at game entry.
    assert offers[: len(OFFERS_IN_ORDER)] == OFFERS_IN_ORDER, wire_listing(mage)
    sent = events_in_direction(mage, TelnetDirection.SENT)
    sent_negotiations = [event for event in sent if isinstance(event, TelnetNegotiation)]
    assert sent_negotiations, wire_listing(mage)
    assert all(event.command in REFUSAL_COMMANDS for event in sent_negotiations), wire_listing(mage)


@pytest.mark.characters(MAGE)
def test_an_msdp_client_completes_the_ttype_cycle_and_accepts_iso_8859_1(server) -> None:
    mage = server.connect(MAGE.name, client=MSDP_CLIENT)
    mage.expect_subnegotiation(OPTION_CHARSET)
    mage.login()
    received = events_in_direction(mage, TelnetDirection.RECEIVED)
    sent = events_in_direction(mage, TelnetDirection.SENT)
    ttype_requests = [payload for payload in subnegotiation_payloads(received, OPTION_TTYPE) if payload == TTYPE_SEND_PAYLOAD]
    assert len(ttype_requests) == MSDP_CLIENT_TTYPE_REQUESTS, wire_listing(mage)
    assert subnegotiation_payloads(sent, OPTION_TTYPE) == MSDP_CLIENT_TTYPE_REPLIES, wire_listing(mage)
    assert subnegotiation_payloads(sent, OPTION_CHARSET) == MSDP_CLIENT_CHARSET_REPLIES, wire_listing(mage)


@pytest.mark.characters(MAGE)
def test_an_msdp_client_lists_commands_at_the_login_prompt(server) -> None:
    mage = server.connect(MAGE.name, client=MSDP_CLIENT)
    mage.expect_subnegotiation(OPTION_CHARSET)
    mage.send_msdp("LIST", "COMMANDS")
    reply = mage.expect_subnegotiation(OPTION_MSDP, matching=sets_msdp_commands)
    assert decode_msdp(reply.event.payload) == MSDP_COMMANDS_REPLY, wire_listing(mage)


@pytest.mark.characters(MAGE)
def test_an_msdp_client_hides_the_password_and_gets_its_room_vnum_in_game(server) -> None:
    mage = server.login(MAGE.name, client=MSDP_CLIENT)
    # The password prompt's WILL ECHO; the wait fails the test when it never arrived.
    mage.expect_negotiation(WILL, OPTION_ECHO)
    assert MSDP_ON_REPLY in mage.command(MSDP_ON_COMMAND).text
    room_frame = mage.expect_subnegotiation(OPTION_MSDP, matching=sets_room_vnum)
    assert decode_msdp(room_frame.event.payload)[ROOM_VNUM_VARIABLE] == str(world.ROOM_ARENA_CENTRE), wire_listing(mage)


@pytest.mark.characters(MAGE)
def test_a_full_client_receives_mssp_and_the_mxp_acknowledgement(server) -> None:
    mage = server.connect(MAGE.name, client=FULL_CLIENT)
    status = mage.expect_subnegotiation(OPTION_MSSP)
    assert decode_mssp(status.event.payload) == SERVER_STATUS, wire_listing(mage)
    # The empty SB MXP that acknowledges WILL MXP; the wait fails the test when it never arrived.
    mage.expect_subnegotiation(OPTION_MXP, matching=is_empty)
    mage.login()
    assert MXP_SECURE_CHANNEL in mage.everything_raw, wire_listing(mage)
