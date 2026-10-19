"""The banker (mob program 34): an account's vault per side, deposits and withdrawals of items
and coins, the vault file on disk, and the banker's refusal of gifts and attacks."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession, Transcript

pytestmark = [pytest.mark.scenario, pytest.mark.bank]

# A level-20 human warrior in the bank hall: a mortal of the light side, which the banker serves.
DEPOSITOR = Character(
    name="Testsaver",
    race="human",
    sex="male",
    level=20,
    load_room=world.ROOM_BANK_HALL,
    class_name="warrior",
)
# A level-20 dwarf warrior in the bank hall: another light-side character of the same account.
KINSMAN = Character(
    name="Testkin",
    race="dwarf",
    sex="male",
    level=20,
    load_room=world.ROOM_BANK_HALL,
    class_name="warrior",
)
# A level-20 uruk-hai warrior in the bank hall: a character of the same account on the dark side.
OUTSIDER = Character(
    name="Testuruk",
    race="uruk-hai",
    sex="male",
    level=20,
    load_room=world.ROOM_BANK_HALL,
    class_name="warrior",
)

# The leather cap's virtual number in the test world (tests/integration/world/obj/11.obj).
CAP_VNUM = 1137
# The harness token's virtual number in the test world (tests/integration/world/obj/11.obj).
TOKEN_VNUM = 1130
# The copper the implementor gives the depositor: 10 gold (COPP_IN_GOLD is 1000, structs.h).
PURSE_COPPER = 10000

# What the depositor reads when the banker takes the cap (bank_deposit, banker.cpp).
DEPOSITED_CAP_MARKER = "You hand over a leather cap."
# What the depositor reads when the banker gives the cap back (bank_withdraw, banker.cpp).
WITHDRAWN_CAP_MARKER = "You are handed a leather cap."
# The banker's tell when no slot matches the withdrawal (bank_withdraw).
NOTHING_LIKE_THAT_MARKER = "A banker tells you 'I hold nothing like that for you.'"
# The banker's tell when a coin deposit is more than the purse holds (bank_deposit_coins).
PURSE_SHORT_MARKER = "A banker tells you 'You don't have that much.'"
# The banker's tell when a coin withdrawal is more than the vault holds (bank_withdraw_coins).
VAULT_SHORT_MARKER = "A banker tells you 'You don't have that much with me.'"
# The banker's tell when a gift is offered (SPECIAL(banker), banker.cpp).
NO_GIFTS_MARKER = "A banker tells you 'I don't take gifts.'"
# The banker's tell when it is attacked (keeper_protection, shopkeeper.cpp).
NO_ATTACKS_MARKER = "A banker tells you 'Don't even think about it.'"
# What the attacker reads before the blow (do_hit, act_offe.cpp).
ATTACK_MARKER = "You attack a banker!"

# The balance header, then its coin and slot lines (banker_balance and format_bank_balance,
# banker.cpp). The limits are the defaults in game_boot_options.cpp, since the harness lib's
# lib/misc/game_boot_options.json sets only daily_reboot_hour_utc. No fee is configured, so
# the item table has no fee column.
BALANCE_HEADER = "Your vault:"
EMPTY_VAULT_LINES = (
    BALANCE_HEADER,
    "Coins: 0 copper (limit 1000 gold)",
    "Slots: 0 of 10 used",
)
ONE_CAP_VAULT_LINES = (
    BALANCE_HEADER,
    "Coins: 0 copper (limit 1000 gold)",
    "Slots: 1 of 10 used",
    "",
    " #  Item",
    " 1  a leather cap",
)
# The vault file of a light-side character (bank_side_file_name, banker.cpp), kept in the
# account's own folder beside account.json (vault_location, account_character_directory).
LIGHT_VAULT_FILE_NAME = "vault_light.json"
ACCOUNT_FILE_NAME = "account.json"


def reply_text(reply: Transcript) -> str:
    """The reply with the carriage returns of the game's line ends removed."""
    return reply.text.replace("\r", "")


def assert_lines(reply: Transcript, lines: tuple) -> None:
    """Asserts the reply holds lines, consecutively and in order."""
    block = "\n".join(lines)
    assert block in reply_text(reply), f"expected:\n{block}\nin:\n{reply.text}"


def hand_object(implementor: GameSession, vnum: int, keyword: str, recipient: Character) -> None:
    """Loads an object into the implementor's inventory and gives it to recipient.

    The implementor must stand in the recipient's room.
    """
    created = implementor.command(f"load obj {vnum}")
    assert created.contains("You create"), created.text
    given = implementor.command(f"give {keyword} {recipient.name.lower()}")
    assert given.contains(f"to {recipient.name}."), given.text


def give_cap_in_bank_hall(implementor: GameSession, recipient: Character) -> None:
    """Brings the implementor to the bank hall and gives recipient a leather cap there."""
    arrived = implementor.command(f"goto {world.ROOM_BANK_HALL}")
    assert arrived.room_name() == "Bank Hall", arrived.text
    hand_object(implementor, CAP_VNUM, "cap", recipient)


def account_folder(lib_dir: Path) -> Path:
    """The harness account's folder: the one folder under lib/accounts holding an account file."""
    account_files = sorted((lib_dir / "accounts").glob(f"*/*/{ACCOUNT_FILE_NAME}"))
    assert len(account_files) == 1, account_files
    return account_files[0].parent


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, DEPOSITOR)
def test_a_banker_keeps_an_item_lists_it_and_hands_it_back(server) -> None:
    """Pins depositing, listing and withdrawing an item at a banker (PR #369).

    `deposit cap` takes the cap from loose inventory into the vault, `balance` lists it as slot
    1, `withdraw 1` hands it back, and a second `withdraw 1` finds the slot gone. The banker sets
    no fee, so no withdrawal depends on the wall-clock bank day; nothing on this path draws.
    """
    implementor = server.login(IMPLEMENTOR.name)
    depositor = server.login(DEPOSITOR.name)
    give_cap_in_bank_hall(implementor, DEPOSITOR)

    assert depositor.command("deposit cap").contains(DEPOSITED_CAP_MARKER)
    assert "a leather cap" not in depositor.command("inventory").text
    assert_lines(depositor.command("balance"), ONE_CAP_VAULT_LINES)

    assert depositor.command("withdraw 1").contains(WITHDRAWN_CAP_MARKER)
    assert depositor.command("inventory").contains("a leather cap")
    assert_lines(depositor.command("balance"), EMPTY_VAULT_LINES)

    assert depositor.command("withdraw 1").contains(NOTHING_LIKE_THAT_MARKER)


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, DEPOSITOR)
def test_a_banker_keeps_coins_up_to_what_the_purse_and_the_vault_hold(server) -> None:
    """Pins coin deposits and withdrawals at a banker, and their refusals (PR #369).

    With 10 gold in the purse, `deposit 20 gold` is refused and `deposit 5 gold` is taken; with
    5 gold in the vault, `withdraw 9 gold` is refused and `withdraw 2 gold` is paid out. score
    shows the purse after each change. Nothing on this path draws.
    """
    implementor = server.login(IMPLEMENTOR.name)
    depositor = server.login(DEPOSITOR.name)
    set_gold = implementor.command(f"wizset player {DEPOSITOR.name.lower()} gold {PURSE_COPPER}")
    assert set_gold.contains(f"gold set to {PURSE_COPPER}."), set_gold.text
    assert depositor.command("score").contains("Gold: 10,")

    assert depositor.command("deposit 20 gold").contains(PURSE_SHORT_MARKER)
    assert depositor.command("deposit 5 gold").contains("You deposit 5 gold.")
    assert depositor.command("score").contains("Gold: 5,")
    assert depositor.command("balance").contains("Coins: 5 gold (limit 1000 gold)")

    assert depositor.command("withdraw 9 gold").contains(VAULT_SHORT_MARKER)
    assert depositor.command("withdraw 2 gold").contains("You withdraw 2 gold.")
    assert depositor.command("score").contains("Gold: 7,")
    assert depositor.command("balance").contains("Coins: 3 gold (limit 1000 gold)")


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, DEPOSITOR, KINSMAN, OUTSIDER)
def test_an_accounts_characters_share_one_vault_per_side(server) -> None:
    """Pins that a vault belongs to the account and the side, not the character (PR #369).

    All three mortals belong to the harness account (the implementor, above level 91, lets
    them be online together). After the human deposits the cap, the dwarf, also of the light
    side, sees it in its own `balance`; the uruk-hai, of the dark side, sees an empty vault.
    Nothing on this path draws.
    """
    implementor = server.login(IMPLEMENTOR.name)
    depositor = server.login(DEPOSITOR.name)
    kinsman = server.login(KINSMAN.name)
    outsider = server.login(OUTSIDER.name)
    give_cap_in_bank_hall(implementor, DEPOSITOR)
    assert depositor.command("deposit cap").contains(DEPOSITED_CAP_MARKER)

    assert_lines(kinsman.command("balance"), ONE_CAP_VAULT_LINES)

    outsider_balance = outsider.command("balance")
    assert_lines(outsider_balance, EMPTY_VAULT_LINES)
    assert "a leather cap" not in outsider_balance.text, outsider_balance.text


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, DEPOSITOR)
def test_a_deposited_item_is_written_to_the_accounts_vault_file(server) -> None:
    """Pins that a deposit writes the light side's vault file in the account's folder (PR #369).

    The vault is written before the character is saved, so the file holds the cap as soon as
    the deposit message arrives: one slot whose top object (nesting depth 0, kept in wear_pos)
    is the cap, and no coins. Nothing on this path draws.
    """
    implementor = server.login(IMPLEMENTOR.name)
    depositor = server.login(DEPOSITOR.name)
    vault_path = account_folder(server.lib_dir) / LIGHT_VAULT_FILE_NAME
    assert not vault_path.exists(), f"{vault_path} exists before any deposit"
    give_cap_in_bank_hall(implementor, DEPOSITOR)

    assert depositor.command("deposit cap").contains(DEPOSITED_CAP_MARKER)

    vault = json.loads(vault_path.read_text(encoding="utf-8"))
    assert (vault["version"], vault["coins"], len(vault["slots"])) == (1, 0, 1), vault
    stored_objects = vault["slots"][0]["objects"]
    assert [(stored["item_number"], stored["wear_pos"]) for stored in stored_objects] == [(CAP_VNUM, 0)], vault


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, DEPOSITOR)
def test_a_banker_refuses_a_gift(server) -> None:
    """Pins that a banker refuses what a player gives it, and the player keeps it (PR #369).

    SPECIAL(banker) answers `give` aimed at itself with a tell before do_give runs. Nothing on
    this path draws.
    """
    implementor = server.login(IMPLEMENTOR.name)
    depositor = server.login(DEPOSITOR.name)
    arrived = implementor.command(f"goto {world.ROOM_BANK_HALL}")
    assert arrived.room_name() == "Bank Hall", arrived.text
    hand_object(implementor, TOKEN_VNUM, "token", DEPOSITOR)

    assert depositor.command("give token banker").contains(NO_GIFTS_MARKER)
    assert depositor.command("inventory").contains("a harness token")


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(DEPOSITOR)
def test_a_banker_refuses_an_attack(server) -> None:
    """Pins that a banker cancels an attack on it with a tell (PR #369's shared keeper protection).

    `kill banker` reaches hit() (fight.cpp). Whatever its rolls decide, every path asks the
    banker's program the SPECIAL_DAMAGE question before any damage lands: a blow through
    damage(), and a miss, dodge or parry after it. The only roll-decided path that skips it is a
    riposte, which needs a wielded weapon the banker does not have (check_riposte), so the
    refusal does not depend on a roll. keeper_protection (shopkeeper.cpp) answers with the tell.
    """
    depositor = server.login(DEPOSITOR.name)

    reply = depositor.command("kill banker")
    assert reply.contains(ATTACK_MARKER), reply.text
    assert reply.contains(NO_ATTACKS_MARKER), reply.text
