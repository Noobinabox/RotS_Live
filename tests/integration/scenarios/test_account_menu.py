"""The account menu before a character is chosen: logging out, the reset menu after wrong
passwords, the failed-login notice, and the character roster's selection, sort and filter keys."""

from __future__ import annotations

import contextlib
import re
from typing import Iterator

import pytest

from rots_harness import world
from rots_harness.characters import HARNESS_EMAIL, HARNESS_PASSWORD, IMPLEMENTOR, Character
from rots_harness.launcher import LOGIN_EMAIL_PROMPT
from rots_harness.session import (
    ACCOUNT_MENU_PLAY,
    ACCOUNT_MENU_PROMPT,
    CHARACTER_CHOICE_PROMPT,
    CHARACTER_MENU_ENTER,
    CHARACTER_MENU_PROMPT,
    ENTER_GAME_MARKER,
    LOGIN_PASSWORD_PROMPT,
    GameSession,
)

pytestmark = [pytest.mark.scenario, pytest.mark.account_menu]

# A password the harness account does not have.
WRONG_PASSWORD = "Wrongpass9"
# A reset code that can never match: the game's codes are six digits
# (generate_numeric_verification_code, account_management.cpp).
WRONG_RESET_CODE = "notacode"
# The account menu's log-out choice (show_account_menu, interpre.cpp).
ACCOUNT_MENU_LOG_OUT = "0"
# The reset menu's reset choice (CON_ACCTPWDFAIL in nanny, interpre.cpp).
RESET_MENU_RESET = "1"
# Wrong passwords before the reset menu, and wrong reset codes before the connection closes
# (nanny, interpre.cpp).
ATTEMPTS_ALLOWED = 5
# Seconds a connection is given to close after its last line: the game loop closes it in the
# CON_CLOSE sweep of a later pass (game_loop, comm.cpp).
CLOSE_SECONDS = 3.0

# What account log-out prints before the connection closes (CON_ACCTMENU in nanny, interpre.cpp).
GOODBYE_MARKER = "Goodbye."
# What every wrong account password prints (CON_ACCTPWD in nanny, interpre.cpp).
INVALID_CREDENTIALS_MARKER = "Invalid account credentials."
# The reset menu shown instead of the password prompt after the last wrong password
# (show_account_reset_menu, interpre.cpp).
RESET_MENU_LINES = (
    "1) Reset your account password",
    "0) Disconnect",
    "This menu will close in 90 seconds.",
)
# What a reset request prints whether or not a code was sent (CON_ACCTPWDFAIL in nanny).
RESET_REQUESTED_LINES = (
    "If an account exists for that address, a reset code has been sent to it.",
    "The code is valid for 15 minutes.",
)
# The prompt for the emailed reset code (CON_ACCTPWDFAIL and CON_ACCTFORGOTCODE in nanny).
RESET_CODE_PROMPT = "Reset code:"
# What each wrong reset code but the last prints (CON_ACCTFORGOTCODE in nanny).
INVALID_RESET_CODE_MARKER = "That reset code is invalid."
# What the last wrong reset code prints before the connection closes (CON_ACCTFORGOTCODE).
TOO_MANY_RESET_CODES_LINES = ("Too many invalid reset codes.", "Please reconnect and try again.")
# The notice a login shows after two or more failed attempts; format with the count
# (format_account_login_failure_notice, account_management_presentation.cpp).
FAILED_LOGIN_NOTICE_FORMAT = "{} FAILED LOGIN ATTEMPTS SINCE YOUR LAST SUCCESSFUL LOGIN."
# The words every failed-login notice contains, whatever the count.
FAILED_LOGIN_NOTICE_WORDS = "SINCE YOUR LAST SUCCESSFUL LOGIN."
# What the roster prints for a selection that names no linked character
# (select_linked_character, account_management_identity.cpp).
ROSTER_SELECTION_ERROR = "Select a linked character by number or name, or enter 0 to return to the account menu."
# One roster row, `%zu) [%3d %s] %-12.12s`: row, level, race, name
# (format_account_character_short_entry, account_management.cpp).
ROSTER_ROW_PATTERN = re.compile(r"(\d+)\) \[ *(\d+) (\w+)\] (\w+)")
# The side sort's section headers (side_section_label, account_management.cpp).
GOOD_SECTION_HEADER = "-- Good --"
EVIL_SECTION_HEADER = "-- Evil --"
# Side names the roster's headers must never use.
FORBIDDEN_SIDE_WORDS = ("Lights", "Darks")

# The roster's characters, in the order the account links them: the marker's order. Each sort
# key and each filter key shows them in an order no other key gives.
ROSTER_MAGE = Character(
    name="Tarwen", race="wood elf", sex="female", level=30, load_room=world.ROOM_ARENA_CENTRE, class_name="mage"
)
ROSTER_ORC = Character(
    name="Grishak", race="orc", sex="male", level=18, load_room=world.ROOM_ARENA_CENTRE, class_name="warrior"
)
# The ranger alone starts in Arena West, so entering the game shows which character was selected.
ROSTER_RANGER = Character(
    name="Aswin", race="human", sex="male", level=25, load_room=world.ROOM_ARENA_WEST, class_name="ranger"
)
ROSTER_URUK = Character(
    name="Mordak", race="uruk-hai", sex="male", level=12, load_room=world.ROOM_ARENA_CENTRE, class_name="warrior"
)
ROSTER_MYSTIC = Character(
    name="Helwyn", race="human", sex="female", level=8, load_room=world.ROOM_ARENA_CENTRE, class_name="mystic"
)
ROSTER = (ROSTER_MAGE, ROSTER_ORC, ROSTER_RANGER, ROSTER_URUK, ROSTER_MYSTIC)
# The account's link order, which the roster shows until a sort key is pressed.
LINK_ORDER = [character.name for character in ROSTER]
# Level order, highest first (ordered_roster_indices, account_management.cpp).
LEVEL_ORDER = ["Tarwen", "Aswin", "Grishak", "Mordak", "Helwyn"]


@contextlib.contextmanager
def account_connection(server, label: str) -> Iterator[GameSession]:
    """A connection at the account email prompt, closed when the block ends.

    It plays no character, so the server fixture does not quit it; label names its transcript.
    """
    connection = GameSession(server.handle, label, HARNESS_EMAIL, HARNESS_PASSWORD, server.run_dir)
    try:
        connection.expect([LOGIN_EMAIL_PROMPT])
        yield connection
    finally:
        connection.close()


def answer_password(connection: GameSession, password: str) -> str:
    """Sends password at the password prompt; returns the reply up to the next prompt."""
    connection.send_line(password)
    return connection.expect([LOGIN_PASSWORD_PROMPT, ACCOUNT_MENU_PROMPT])


def enter_email(connection: GameSession) -> None:
    """Sends the harness email at the email prompt and waits for the password prompt."""
    connection.send_line(HARNESS_EMAIL)
    connection.expect([LOGIN_PASSWORD_PROMPT])


def sign_in(connection: GameSession) -> str:
    """Signs in with the harness account; returns everything read up to the account menu."""
    enter_email(connection)
    reply = answer_password(connection, HARNESS_PASSWORD)
    assert ACCOUNT_MENU_PROMPT in reply, reply
    return reply


def open_roster(connection: GameSession) -> str:
    """Signs in, chooses the roster from the account menu and returns its first rendering."""
    sign_in(connection)
    connection.send_line(ACCOUNT_MENU_PLAY)
    return connection.expect([CHARACTER_CHOICE_PROMPT])


def roster_answer(connection: GameSession, text: str) -> str:
    """Sends text at the roster prompt and returns the reply up to the next prompt."""
    connection.send_line(text)
    return connection.expect([CHARACTER_CHOICE_PROMPT, CHARACTER_MENU_PROMPT, ACCOUNT_MENU_PROMPT])


def roster_names(roster_text: str) -> list:
    """The character names of a roster rendering, in the order shown."""
    return [match[3] for match in ROSTER_ROW_PATTERN.findall(roster_text)]


def closes_within(connection: GameSession, seconds: float) -> bool:
    """Whether the server closes the connection within seconds; text read meanwhile is discarded."""
    try:
        connection.drain(seconds)
    except ConnectionError:
        return True
    return False


def exhaust_passwords(connection: GameSession) -> list:
    """Enters the harness email, then wrong passwords until the attempts run out; returns each reply."""
    enter_email(connection)
    return [answer_password(connection, WRONG_PASSWORD) for _attempt in range(ATTEMPTS_ALLOWED)]


@pytest.mark.characters(IMPLEMENTOR)
def test_logging_out_of_the_account_menu_says_goodbye_and_closes_the_connection(server) -> None:
    """Pins #279: the account menu's 0 prints Goodbye. and then the server closes the connection."""
    with account_connection(server, "logout") as connection:
        sign_in(connection)
        connection.send_line(ACCOUNT_MENU_LOG_OUT)
        farewell = connection.expect([GOODBYE_MARKER, ACCOUNT_MENU_PROMPT])
        assert GOODBYE_MARKER in farewell, farewell
        assert closes_within(connection, CLOSE_SECONDS), f"the connection stayed open after {GOODBYE_MARKER!r}"


@pytest.mark.characters(IMPLEMENTOR)
def test_the_fifth_wrong_password_opens_the_reset_menu(server) -> None:
    """Pins #284: the first four wrong passwords prompt again; the fifth shows the reset menu instead."""
    with account_connection(server, "resetmenu") as connection:
        replies = exhaust_passwords(connection)
        for earlier_reply in replies[:-1]:
            assert INVALID_CREDENTIALS_MARKER in earlier_reply, earlier_reply
            assert LOGIN_PASSWORD_PROMPT in earlier_reply, earlier_reply
            assert RESET_MENU_LINES[0] not in earlier_reply, earlier_reply
        last_reply = replies[-1]
        assert INVALID_CREDENTIALS_MARKER in last_reply, last_reply
        assert LOGIN_PASSWORD_PROMPT not in last_reply, last_reply
        for menu_line in RESET_MENU_LINES:
            assert menu_line in last_reply, last_reply


@pytest.mark.characters(IMPLEMENTOR)
def test_five_wrong_reset_codes_end_the_reset_and_close_the_connection(server) -> None:
    """Pins #284: after a reset request, four wrong codes prompt again and the fifth closes the connection.

    The request's reply is the same whether or not a code was sent; the harness's server has no
    mail program, so none is.
    """
    with account_connection(server, "resetcode") as connection:
        replies = exhaust_passwords(connection)
        assert RESET_MENU_LINES[0] in replies[-1], replies[-1]
        connection.send_line(RESET_MENU_RESET)
        requested = connection.expect([RESET_CODE_PROMPT])
        for requested_line in RESET_REQUESTED_LINES:
            assert requested_line in requested, requested

        for _attempt in range(ATTEMPTS_ALLOWED - 1):
            connection.send_line(WRONG_RESET_CODE)
            reply = connection.expect([RESET_CODE_PROMPT, TOO_MANY_RESET_CODES_LINES[1]])
            assert INVALID_RESET_CODE_MARKER in reply and RESET_CODE_PROMPT in reply, reply
        connection.send_line(WRONG_RESET_CODE)
        last_reply = connection.expect([RESET_CODE_PROMPT, TOO_MANY_RESET_CODES_LINES[1]])
        for closing_line in TOO_MANY_RESET_CODES_LINES:
            assert closing_line in last_reply, last_reply
        assert RESET_CODE_PROMPT not in last_reply, last_reply
        assert closes_within(connection, CLOSE_SECONDS), "the connection stayed open after the last wrong code"


@pytest.mark.characters(IMPLEMENTOR)
def test_failed_logins_are_reported_at_the_next_successful_login(server) -> None:
    """Pins #284: a login after two wrong passwords opens with the count of failed attempts.

    The notice's next line names the address the attempts came from, which differs between
    launchers, so only the count line is asserted.
    """
    failed_attempts = 2
    with account_connection(server, "notice") as connection:
        enter_email(connection)
        for _attempt in range(failed_attempts):
            reply = answer_password(connection, WRONG_PASSWORD)
            assert INVALID_CREDENTIALS_MARKER in reply, reply
        login = answer_password(connection, HARNESS_PASSWORD)
        assert ACCOUNT_MENU_PROMPT in login, login
        assert FAILED_LOGIN_NOTICE_FORMAT.format(failed_attempts) in login, login


@pytest.mark.characters(IMPLEMENTOR)
def test_the_failed_login_notice_is_shown_only_once(server) -> None:
    """Pins #284: the login that shows the failed-login notice clears the count; the next shows none."""
    with account_connection(server, "noticefirst") as connection:
        enter_email(connection)
        wrong = answer_password(connection, WRONG_PASSWORD)
        assert INVALID_CREDENTIALS_MARKER in wrong, wrong
        first_login = answer_password(connection, HARNESS_PASSWORD)
        assert FAILED_LOGIN_NOTICE_WORDS in first_login, first_login
        connection.send_line(ACCOUNT_MENU_LOG_OUT)
        connection.expect([GOODBYE_MARKER])
    with account_connection(server, "noticesecond") as connection:
        second_login = sign_in(connection)
        assert FAILED_LOGIN_NOTICE_WORDS not in second_login, second_login


@pytest.mark.characters(*ROSTER)
def test_the_roster_selects_a_character_by_its_full_name_in_any_case(server) -> None:
    """Pins #285: a linked character's full name selects it at the roster, whatever the letters' case."""
    with account_connection(server, "byname") as connection:
        open_roster(connection)
        reply = roster_answer(connection, "aSWIN")
        assert CHARACTER_MENU_PROMPT in reply and ROSTER_SELECTION_ERROR not in reply, reply
        connection.send_line(CHARACTER_MENU_ENTER)
        connection.expect([ENTER_GAME_MARKER])
        assert connection.command("look").room_name() == "Arena West"
        connection.quit()


@pytest.mark.characters(*ROSTER)
def test_the_roster_rejects_the_start_of_a_name(server) -> None:
    """Pins #285: selecting by name needs the whole name; its first letters select nobody."""
    with account_connection(server, "prefix") as connection:
        open_roster(connection)
        reply = roster_answer(connection, "Asw")
        assert ROSTER_SELECTION_ERROR in reply and CHARACTER_CHOICE_PROMPT in reply, reply
        assert CHARACTER_MENU_PROMPT not in reply, reply


@pytest.mark.parametrize(
    ("sort_key", "expected_names"),
    [
        pytest.param("A", ["Aswin", "Grishak", "Helwyn", "Mordak", "Tarwen"], id="name"),
        pytest.param("L", LEVEL_ORDER, id="level"),
        pytest.param("C", ["Aswin", "Helwyn", "Tarwen", "Mordak", "Grishak"], id="race"),
    ],
)
@pytest.mark.characters(*ROSTER)
def test_a_sort_key_reorders_the_roster(server, sort_key: str, expected_names: list) -> None:
    """Pins #291: the roster shows the account's link order until a sort key reorders it.

    Race order follows the game's race numbers (human 1, wood elf 3, uruk-hai 11, orc 13), in
    link order between equals. The side key has its own test, with its headers.
    """
    with account_connection(server, f"sort{sort_key.lower()}") as connection:
        first_rendering = open_roster(connection)
        assert roster_names(first_rendering) == LINK_ORDER, first_rendering
        sorted_rendering = roster_answer(connection, sort_key)
        assert roster_names(sorted_rendering) == expected_names, sorted_rendering


@pytest.mark.characters(*ROSTER)
def test_the_side_sort_lists_good_then_evil_under_their_headers(server) -> None:
    """Pins #291 and #296: the side key lists Good, then Evil, A-Z within each, under -- Good -- and
    -- Evil -- headers that never say Lights or Darks."""
    with account_connection(server, "sideheaders") as connection:
        open_roster(connection)
        rendering = roster_answer(connection, "S")
        assert GOOD_SECTION_HEADER in rendering and EVIL_SECTION_HEADER in rendering, rendering
        good_section, evil_section = rendering.split(EVIL_SECTION_HEADER)
        assert roster_names(good_section) == ["Aswin", "Helwyn", "Tarwen"], rendering
        assert roster_names(evil_section) == ["Grishak", "Mordak"], rendering
        for forbidden_word in FORBIDDEN_SIDE_WORDS:
            assert forbidden_word not in rendering, rendering


@pytest.mark.parametrize(
    ("filter_key", "label", "expected_names"),
    [
        pytest.param("W", "Warrior", ["Grishak", "Mordak"], id="warrior"),
        pytest.param("R", "Ranger", ["Aswin"], id="ranger"),
        pytest.param("T", "Mystic", ["Helwyn"], id="mystic"),
        pytest.param("M", "Mage", ["Tarwen"], id="mage"),
    ],
)
@pytest.mark.characters(*ROSTER)
def test_a_filter_key_shows_only_that_class_and_counts_them(
    server, filter_key: str, label: str, expected_names: list
) -> None:
    """Pins #291: a filter key lists only the characters whose strongest profession it names, and a
    footer counts them against the whole roster."""
    with account_connection(server, f"filter{filter_key.lower()}") as connection:
        open_roster(connection)
        filtered = roster_answer(connection, filter_key)
        assert roster_names(filtered) == expected_names, filtered
        footer = f"{len(expected_names)} of {len(ROSTER)} characters shown ({label}).  Press {filter_key} to clear."
        assert footer in filtered, filtered


@pytest.mark.characters(*ROSTER)
def test_pressing_a_filter_key_again_clears_the_filter(server) -> None:
    """Pins #291: the active filter's own key clears it and brings every character back."""
    with account_connection(server, "clearfilter") as connection:
        open_roster(connection)
        filtered = roster_answer(connection, "W")
        assert roster_names(filtered) == ["Grishak", "Mordak"], filtered
        cleared = roster_answer(connection, "W")
        assert roster_names(cleared) == LINK_ORDER, cleared
        assert f"{len(ROSTER)} characters displayed." in cleared, cleared


@pytest.mark.characters(*ROSTER)
def test_the_roster_keeps_its_sort_in_the_next_session(server) -> None:
    """Pins #291: a sort chosen before leaving the roster is the order the account's next session shows first."""
    with account_connection(server, "sortfirst") as connection:
        open_roster(connection)
        level_sorted = roster_answer(connection, "L")
        assert roster_names(level_sorted) == LEVEL_ORDER, level_sorted
        back_at_menu = roster_answer(connection, "0")
        assert ACCOUNT_MENU_PROMPT in back_at_menu, back_at_menu
        connection.send_line(ACCOUNT_MENU_LOG_OUT)
        connection.expect([GOODBYE_MARKER])
    with account_connection(server, "sortsecond") as connection:
        rendering = open_roster(connection)
        assert roster_names(rendering) == LEVEL_ORDER, rendering
