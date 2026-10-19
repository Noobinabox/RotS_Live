"""The account store from inside the game: deleting a linked character, the immortal account
reports on a clean boot, and display settings shared by an account's characters."""

from __future__ import annotations

import pytest

from rots_harness import world
from rots_harness.characters import HARNESS_EMAIL, IMPLEMENTOR, Character
from rots_harness.session import GameSession

pytestmark = [pytest.mark.scenario, pytest.mark.account_menu]

# A level-12 human warrior linked to the harness account, which the implementor deletes; never online.
VICTIM = Character(
    name="Testdoomed", race="human", sex="male", level=12, load_room=world.ROOM_ARENA_CENTRE, class_name="warrior"
)
# A level-20 human warrior on the harness account, online beside the implementor.
MORTAL = Character(
    name="Testsibling", race="human", sex="female", level=20, load_room=world.ROOM_ARENA_CENTRE, class_name="warrior"
)

# The password do_delete requires after the character's name (act_wiz.cpp).
DELETE_PASSWORD = "rots"
# What do_delete prints when it refuses (act_wiz.cpp); on success it prints nothing.
DELETE_REFUSALS = (
    "Incorrect or missing password",
    "No such player",
    "That character could not be deleted; check the log.",
)
# The harness account's folder in the lib, accounts/<letter range>/<email>, as rotstool writes it
# for the server's account layer (account_management.cpp).
ACCOUNT_FOLDER = f"accounts/F-J/{HARNESS_EMAIL}"
# The folder delete_player_character_by_index copies a linked character's file into (db.cpp).
ARCHIVE_FOLDER = "players/ZZZ"
# The start of the character list line of `account show`
# (format_account_summary, account_management_presentation.cpp).
ACCOUNT_SHOW_CHARACTERS_PREFIX = "Characters ("

# What `account index` reports when the boot indexed the one account cleanly (do_account, act_wiz.cpp).
CLEAN_INDEX_REPORT = "Account index: 1 record(s) indexed, 0 quarantined, 0 unreadable since boot."
# What `account errors` reports when nothing has been recorded since boot (do_account).
NO_ACCOUNT_ERRORS_REPORT = "No account errors recorded since boot."

# What `set brief on` prints to the character who types it (tog_messages, act_othe.cpp).
BRIEF_TURNED_ON = "Brief mode now on."
# What `set brief` without an argument prints for each state (flag_void, act_othe.cpp).
BRIEF_IS_ON = "Brief mode on."
BRIEF_IS_OFF = "Brief mode off."


def account_show_characters(implementor: GameSession) -> str:
    """The character list line `account show` prints for the harness account."""
    reply = implementor.command(f"account show {HARNESS_EMAIL}").text
    lines = [line.strip() for line in reply.splitlines() if line.strip().startswith(ACCOUNT_SHOW_CHARACTERS_PREFIX)]
    assert len(lines) == 1, reply
    return lines[0]


@pytest.mark.characters(IMPLEMENTOR, VICTIM)
def test_deleting_a_linked_character_unlinks_it_and_archives_its_file(server) -> None:
    """Pins #282: `delete` takes a linked character off its account and out of the account's folder,
    keeping a copy of its file in players/ZZZ."""
    implementor = server.login(IMPLEMENTOR.name)
    victim_key = VICTIM.name.lower()
    account_folder = server.lib_dir / ACCOUNT_FOLDER
    victim_files = [account_folder / f"{victim_key}.{kind}.json" for kind in ("character", "objects", "exploits")]
    for victim_file in victim_files:
        assert victim_file.is_file(), f"missing before the delete: {victim_file}"
    before = account_show_characters(implementor)
    assert before == f"Characters (2): {IMPLEMENTOR.name.lower()}, {victim_key}", before

    reply = implementor.command(f"delete {victim_key} {DELETE_PASSWORD}")
    for refusal in DELETE_REFUSALS:
        assert refusal not in reply.text, reply.text

    after = account_show_characters(implementor)
    assert after == f"Characters (1): {IMPLEMENTOR.name.lower()}", after
    left_in_account = sorted(entry.name for entry in account_folder.iterdir() if entry.name.startswith(victim_key))
    assert left_in_account == [], left_in_account
    archived = sorted(entry.name for entry in (server.lib_dir / ARCHIVE_FOLDER).iterdir())
    assert archived == [victim_key], archived


@pytest.mark.characters(IMPLEMENTOR)
def test_account_index_reports_one_clean_record_after_a_clean_boot(server) -> None:
    """Pins #297: `account index` counts the harness account's record, none quarantined or unreadable."""
    implementor = server.login(IMPLEMENTOR.name)
    reply = implementor.command("account index")
    assert CLEAN_INDEX_REPORT in reply.text, reply.text


@pytest.mark.characters(IMPLEMENTOR)
def test_account_errors_reports_none_after_a_clean_boot(server) -> None:
    """Pins #297: `account errors` on a boot with nothing recorded says so instead of printing an empty list."""
    implementor = server.login(IMPLEMENTOR.name)
    reply = implementor.command("account errors")
    assert NO_ACCOUNT_ERRORS_REPORT in reply.text, reply.text


@pytest.mark.characters(IMPLEMENTOR, MORTAL)
def test_a_display_setting_changed_by_one_character_shows_on_the_accounts_other_online_character(server) -> None:
    """Pins #294: brief mode turned on by one character is on for the same account's other online character.

    Brief mode is one of the display settings the account holds (PPC_PRF_MASK, structs.h); the
    harness account starts with it off.
    """
    implementor = server.login(IMPLEMENTOR.name)
    mortal = server.login(MORTAL.name)
    before = mortal.command("set brief")
    assert BRIEF_IS_OFF in before.text, before.text

    turned_on = implementor.command("set brief on")
    assert BRIEF_TURNED_ON in turned_on.text, turned_on.text

    after = mortal.command("set brief")
    assert BRIEF_IS_ON in after.text and BRIEF_IS_OFF not in after.text, after.text
