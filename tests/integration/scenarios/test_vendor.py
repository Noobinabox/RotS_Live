"""The barter vendor (mob program 33): its stock list, a refused short payment, a completed
trade, and the vendor's refusal of gifts and attacks."""

from __future__ import annotations

import pytest

from rots_harness import world
from rots_harness.characters import IMPLEMENTOR, Character
from rots_harness.session import GameSession, Transcript

pytestmark = [pytest.mark.scenario, pytest.mark.vendor]

# A level-20 human warrior in the trade hall: a mortal the trader serves.
BUYER = Character(
    name="Testbuyer",
    race="human",
    sex="male",
    level=20,
    load_room=world.ROOM_TRADE_HALL,
    class_name="warrior",
)

# The harness token's virtual number in the test world (tests/integration/world/obj/11.obj),
# the currency of the trader's one price line, `price 1137 1130x2` (tests/integration/world/mob/13.mob).
TOKEN_VNUM = 1130
# How many tokens the trader asks for a leather cap.
CAP_PRICE_TOKENS = 2

# The list header the trader shows without a list= option (VENDOR_DEFAULT_LIST_MESSAGE,
# shopkeeper.h), then the one stock row (format_vendor_list, shopkeeper.cpp): the store room holds
# a leather cap, which zone 13 resets there.
STOCK_LIST_LINES = (
    "What would you like to trade?",
    " 1. a leather cap  2 x a harness token",
)
# The trader's tell when the buyer carries one of the two tokens (vendor_buy, shopkeeper.cpp).
SHORT_PAYMENT_MARKER = "A trader tells you 'You need 2 x a harness token (you have 1).'"
# What the buyer reads when the trade completes (vendor_buy): one line per currency.
TRADE_LINES = (
    "You hand over:",
    "  2 x a harness token",
    "You now have a leather cap.",
)
# The trader's tell when a gift is offered (SPECIAL(barter_vendor), shopkeeper.cpp).
NO_GIFTS_MARKER = "A trader tells you 'I don't take gifts.'"
# The trader's tell when it is attacked (keeper_protection, shopkeeper.cpp).
NO_ATTACKS_MARKER = "A trader tells you 'Don't even think about it.'"
# What the attacker reads before the blow (do_hit, act_offe.cpp).
ATTACK_MARKER = "You attack a trader!"


def reply_text(reply: Transcript) -> str:
    """The reply with the carriage returns of the game's line ends removed."""
    return reply.text.replace("\r", "")


def assert_lines(reply: Transcript, lines: tuple) -> None:
    """Asserts the reply holds lines, consecutively and in order."""
    block = "\n".join(lines)
    assert block in reply_text(reply), f"expected:\n{block}\nin:\n{reply.text}"


def give_tokens_in_trade_hall(implementor: GameSession, count: int) -> None:
    """Brings the implementor to the trade hall and gives the buyer count harness tokens there."""
    arrived = implementor.command(f"goto {world.ROOM_TRADE_HALL}")
    assert arrived.room_name() == "Trade Hall", arrived.text
    for token_number in range(count):
        created = implementor.command(f"load obj {TOKEN_VNUM}")
        assert created.contains("You create a harness token."), created.text
        given = implementor.command(f"give token {BUYER.name.lower()}")
        assert given.contains(f"You give a harness token to {BUYER.name}."), given.text


def carried_token_count(buyer: GameSession) -> int:
    """How many harness tokens the buyer's inventory lists, one line each."""
    lines = reply_text(buyer.command("inventory")).split("\n")
    return sum(1 for line in lines if line.strip() == "a harness token")


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(BUYER)
def test_a_vendor_lists_its_stocked_item_with_its_price(server) -> None:
    """Pins a barter vendor's `list` (PR #330).

    The trader lists each priced item with a copy in its store room once, with each currency's
    quantity and short description. Nothing on this path draws.
    """
    buyer = server.login(BUYER.name)

    assert_lines(buyer.command("list"), STOCK_LIST_LINES)


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, BUYER)
def test_a_vendor_refuses_a_short_payment_and_takes_nothing(server) -> None:
    """Pins that a barter vendor names what is missing and takes nothing (PR #330).

    With one of the two tokens the cap costs, `buy cap` gets one tell naming the shortfall, and
    the buyer keeps the token and gets no cap. Nothing on this path draws.
    """
    implementor = server.login(IMPLEMENTOR.name)
    buyer = server.login(BUYER.name)
    give_tokens_in_trade_hall(implementor, CAP_PRICE_TOKENS - 1)

    assert buyer.command("buy cap").contains(SHORT_PAYMENT_MARKER)
    assert carried_token_count(buyer) == CAP_PRICE_TOKENS - 1
    assert "a leather cap" not in buyer.command("inventory").text


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, BUYER)
def test_a_vendor_trades_its_item_for_the_priced_tokens(server) -> None:
    """Pins a completed barter trade (PR #330).

    With both tokens in loose inventory, `buy cap` takes the two tokens and hands over a cap,
    listing the payment one currency per line. Nothing on this path draws.
    """
    implementor = server.login(IMPLEMENTOR.name)
    buyer = server.login(BUYER.name)
    give_tokens_in_trade_hall(implementor, CAP_PRICE_TOKENS)

    assert_lines(buyer.command("buy cap"), TRADE_LINES)
    assert carried_token_count(buyer) == 0
    assert buyer.command("inventory").contains("a leather cap")


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(IMPLEMENTOR, BUYER)
def test_a_vendor_refuses_a_gift(server) -> None:
    """Pins that a barter vendor refuses what a player gives it, and the player keeps it (PR #330).

    SPECIAL(barter_vendor) answers `give` aimed at itself with a tell before do_give runs.
    Nothing on this path draws.
    """
    implementor = server.login(IMPLEMENTOR.name)
    buyer = server.login(BUYER.name)
    give_tokens_in_trade_hall(implementor, 1)

    assert buyer.command("give token trader").contains(NO_GIFTS_MARKER)
    assert carried_token_count(buyer) == 1


@pytest.mark.schedule("affect-hours")
@pytest.mark.characters(BUYER)
def test_a_vendor_refuses_an_attack(server) -> None:
    """Pins that a barter vendor cancels an attack on it with a tell (PR #330).

    `kill trader` reaches hit() (fight.cpp). Whatever its rolls decide, every path asks the
    trader's program the SPECIAL_DAMAGE question before any damage lands: a blow through
    damage(), and a miss, dodge or parry after it. The only roll-decided path that skips it is a
    riposte, which needs a wielded weapon the trader does not have (check_riposte), so the
    refusal does not depend on a roll. keeper_protection (shopkeeper.cpp) answers with the tell.
    """
    buyer = server.login(BUYER.name)

    reply = buyer.command("kill trader")
    assert reply.contains(ATTACK_MARKER), reply.text
    assert reply.contains(NO_ATTACKS_MARKER), reply.text
