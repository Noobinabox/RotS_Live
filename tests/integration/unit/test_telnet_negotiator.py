from __future__ import annotations

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
    TelnetNegotiation as Neg,
    TelnetSubnegotiation as Sub,
)
from rots_harness.telnet_negotiator import TelnetNegotiator
from rots_harness.telnet_profile import (
    MSDP_CLIENT,
    REFUSING_CLIENT,
    SILENT_CLIENT,
    TelnetClientProfile,
)

# The server's TTYPE SEND request.
TTYPE_SEND_REQUEST = Sub(OPTION_TTYPE, b"\x01")

# The CHARSET request RotS sends: REQUEST, a space separator, then one character set.
LATIN1_CHARSET_REQUEST = Sub(OPTION_CHARSET, b"\x01 ISO-8859-1")


def answers(negotiator: TelnetNegotiator, *events: TelnetEvent) -> list[TelnetEvent]:
    replies: list[TelnetEvent] = []
    for event in events:
        replies.extend(negotiator.respond(event))
    return replies


def test_a_silent_client_never_answers() -> None:
    negotiator = TelnetNegotiator(SILENT_CLIENT)
    assert answers(negotiator, Neg(DO, OPTION_TTYPE)) == []
    assert answers(negotiator, Neg(WILL, OPTION_MSDP)) == []
    assert answers(negotiator, TTYPE_SEND_REQUEST) == []
    assert answers(negotiator, Neg(WILL, OPTION_ECHO)) == []


def test_an_msdp_client_answers_the_server_offers_in_order() -> None:
    negotiator = TelnetNegotiator(MSDP_CLIENT)
    replies = answers(
        negotiator,
        Neg(DO, OPTION_TTYPE),
        TTYPE_SEND_REQUEST,
        Neg(DO, OPTION_NAWS),
        Neg(DO, OPTION_CHARSET),
        Neg(WILL, OPTION_MSDP),
        Neg(WILL, OPTION_MSSP),
        Neg(DO, OPTION_ATCP),
        Neg(WILL, OPTION_MSP),
        Neg(DO, OPTION_MXP),
        LATIN1_CHARSET_REQUEST,
    )
    assert replies == [
        Neg(WILL, OPTION_TTYPE),
        Sub(OPTION_TTYPE, b"\x00ROTS-HARNESS"),
        Neg(WILL, OPTION_NAWS),
        Sub(OPTION_NAWS, b"\x00\x50\x00\x18"),
        Neg(WILL, OPTION_CHARSET),
        Neg(DO, OPTION_MSDP),
        Neg(DONT, OPTION_MSSP),
        Neg(WONT, OPTION_ATCP),
        Neg(DONT, OPTION_MSP),
        Neg(WONT, OPTION_MXP),
        Sub(OPTION_CHARSET, b"\x02ISO-8859-1"),
    ]


def test_the_ttype_cycle_repeats_the_last_name() -> None:
    negotiator = TelnetNegotiator(MSDP_CLIENT)
    answers(negotiator, Neg(DO, OPTION_TTYPE))
    replies = answers(negotiator, *([TTYPE_SEND_REQUEST] * 4))
    assert replies == [
        Sub(OPTION_TTYPE, b"\x00ROTS-HARNESS"),
        Sub(OPTION_TTYPE, b"\x00XTERM-256COLOR"),
        Sub(OPTION_TTYPE, b"\x00MTTS 9"),
        Sub(OPTION_TTYPE, b"\x00MTTS 9"),
    ]


def test_ttype_is_not_answered_before_ttype_is_agreed() -> None:
    negotiator = TelnetNegotiator(MSDP_CLIENT)
    assert answers(negotiator, TTYPE_SEND_REQUEST) == []
    assert answers(negotiator, Neg(DO, OPTION_TTYPE), TTYPE_SEND_REQUEST) == [
        Neg(WILL, OPTION_TTYPE),
        Sub(OPTION_TTYPE, b"\x00ROTS-HARNESS"),
    ]


def test_a_refusing_client_refuses_each_offer_once_and_ignores_the_servers_follow_ups() -> None:
    negotiator = TelnetNegotiator(REFUSING_CLIENT)
    assert answers(negotiator, Neg(DO, OPTION_TTYPE)) == [Neg(WONT, OPTION_TTYPE)]
    assert answers(negotiator, Neg(DO, OPTION_NAWS)) == [Neg(WONT, OPTION_NAWS)]
    assert answers(negotiator, Neg(WILL, OPTION_MSDP)) == [Neg(DONT, OPTION_MSDP)]
    assert answers(negotiator, Neg(WONT, OPTION_MSDP)) == []
    assert answers(negotiator, Neg(DO, OPTION_MXP)) == [Neg(WONT, OPTION_MXP)]
    assert answers(negotiator, Neg(WILL, OPTION_MXP)) == [Neg(DONT, OPTION_MXP)]
    assert answers(negotiator, Neg(DONT, OPTION_MXP)) == []


def test_the_password_echo_sequence_ends_without_a_loop() -> None:
    negotiator = TelnetNegotiator(MSDP_CLIENT)
    assert answers(negotiator, Neg(WILL, OPTION_ECHO)) == [Neg(DO, OPTION_ECHO)]
    assert answers(negotiator, Neg(WILL, OPTION_ECHO)) == []
    assert answers(negotiator, Neg(WONT, OPTION_ECHO)) == [Neg(DONT, OPTION_ECHO)]
    assert answers(negotiator, Neg(WONT, OPTION_ECHO)) == []
    assert answers(negotiator, Neg(WILL, OPTION_ECHO)) == [Neg(DO, OPTION_ECHO)]


def test_charset_is_rejected_when_no_profile_charset_is_offered() -> None:
    profile = TelnetClientProfile(
        name="utf8",
        answers_negotiation=True,
        accepted_options=frozenset({OPTION_CHARSET}),
        charsets=("UTF-8",),
    )
    negotiator = TelnetNegotiator(profile)
    assert answers(negotiator, Neg(DO, OPTION_CHARSET)) == [Neg(WILL, OPTION_CHARSET)]
    assert answers(negotiator, LATIN1_CHARSET_REQUEST) == [Sub(OPTION_CHARSET, b"\x03")]
