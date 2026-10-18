// Pins one regeneration_update() pass: each character gains a FAST_UPDATE_RATE-th of its hourly
// hit points, moves and mana, the fraction is settled by one random roll per point pool, every
// pool is capped at its maximum, moves never drop below zero, and a death from negative regen
// ends the pass. Pins one fast_update() pass: it changes no pool and draws no rolls, and runs the
// Power of Arda for evil races and the breathing and Fame War checks for every character,
// including those after a regen death.

#include "regeneration_fixture.h"

#include <gtest/gtest.h>

namespace {

using namespace regen_test;

// Runs the regeneration and fast-update pins on the shared fixture.
class FastUpdateRegenTest : public RegenerationFixture {
};

TEST_F(FastUpdateRegenTest, APassGivesAPlayerItsHourlyGainsDividedByTheFastUpdateRate)
{
    ASSERT_FLOAT_EQ(hit_gain(&m_player), PLAYER_HOURLY_HIT_GAIN);
    ASSERT_FLOAT_EQ(move_gain(&m_player), PLAYER_HOURLY_MOVE_GAIN);
    ASSERT_FLOAT_EQ(mana_gain(&m_player), PLAYER_HOURLY_MANA_GAIN);
    ASSERT_EQ(static_cast<int>(PLAYER_HOURLY_HIT_GAIN / FAST_UPDATE_RATE),
        PLAYER_WHOLE_HIT_POINTS_PER_PASS);
    ASSERT_EQ(static_cast<int>(PLAYER_HOURLY_MOVE_GAIN / FAST_UPDATE_RATE),
        PLAYER_WHOLE_MOVES_PER_PASS);
    ASSERT_EQ(static_cast<int>(PLAYER_HOURLY_MANA_GAIN / FAST_UPDATE_RATE),
        PLAYER_WHOLE_MANA_PER_PASS);
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();

    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS + PLAYER_WHOLE_HIT_POINTS_PER_PASS)
        << describe_gain(PLAYER_HOURLY_HIT_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_MOVE(&m_player), STARTING_POINTS + PLAYER_WHOLE_MOVES_PER_PASS)
        << describe_gain(PLAYER_HOURLY_MOVE_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_MANA(&m_player), STARTING_POINTS + PLAYER_WHOLE_MANA_PER_PASS)
        << describe_gain(PLAYER_HOURLY_MANA_GAIN, ROLL_ADDS_NOTHING);
    expect_every_queued_roll_drawn();
}

TEST_F(FastUpdateRegenTest, ARollBelowAGainsFractionAddsOneMorePoint)
{
    queue_rolls(ROLL_ADDS_ONE_POINT, 2);

    regeneration_update();

    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS + PLAYER_WHOLE_HIT_POINTS_PER_PASS + 1)
        << describe_gain(PLAYER_HOURLY_HIT_GAIN, ROLL_ADDS_ONE_POINT);
    EXPECT_EQ(GET_MOVE(&m_player), STARTING_POINTS + PLAYER_WHOLE_MOVES_PER_PASS + 1)
        << describe_gain(PLAYER_HOURLY_MOVE_GAIN, ROLL_ADDS_ONE_POINT);
    EXPECT_EQ(GET_MANA(&m_player), STARTING_POINTS + PLAYER_WHOLE_MANA_PER_PASS + 1)
        << describe_gain(PLAYER_HOURLY_MANA_GAIN, ROLL_ADDS_ONE_POINT);
    expect_every_queued_roll_drawn();
}

// The three rolls are chosen so any other draw order changes at least one pool.
TEST_F(FastUpdateRegenTest, RollsAreDrawnForHitPointsThenMovesThenMana)
{
    queue_character_rolls(ROLL_ADDS_NOTHING, ROLL_ADDS_ONLY_TO_PLAYER_MOVES, ROLL_ADDS_ONE_POINT);
    queue_rolls(ROLL_ADDS_NOTHING, 1);

    regeneration_update();

    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS + PLAYER_WHOLE_HIT_POINTS_PER_PASS)
        << describe_gain(PLAYER_HOURLY_HIT_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_MOVE(&m_player), STARTING_POINTS + PLAYER_WHOLE_MOVES_PER_PASS + 1)
        << describe_gain(PLAYER_HOURLY_MOVE_GAIN, ROLL_ADDS_ONLY_TO_PLAYER_MOVES);
    EXPECT_EQ(GET_MANA(&m_player), STARTING_POINTS + PLAYER_WHOLE_MANA_PER_PASS + 1)
        << describe_gain(PLAYER_HOURLY_MANA_GAIN, ROLL_ADDS_ONE_POINT);
    expect_every_queued_roll_drawn();
}

// The player's hit points are already full, yet it still draws three rolls, so the NPC after it
// draws the fourth roll for its hit points.
TEST_F(FastUpdateRegenTest, EachCharacterDrawsThreeRollsEvenForAFullPool)
{
    set_pools(m_player, MAXIMUM_POINTS, STARTING_POINTS, STARTING_POINTS);
    queue_rolls(ROLL_ADDS_NOTHING, 1);
    queue_character_rolls(ROLL_ADDS_ONE_POINT, ROLL_ADDS_NOTHING, ROLL_ADDS_NOTHING);

    regeneration_update();

    EXPECT_EQ(GET_HIT(&m_player), MAXIMUM_POINTS);
    EXPECT_EQ(GET_HIT(&m_npc), STARTING_POINTS + NPC_WHOLE_HIT_POINTS_PER_PASS + 1)
        << describe_gain(NPC_HOURLY_HIT_GAIN, ROLL_ADDS_ONE_POINT);
    EXPECT_EQ(GET_MOVE(&m_npc), STARTING_POINTS + NPC_WHOLE_MOVES_PER_PASS)
        << describe_gain(NPC_HOURLY_MOVE_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_MANA(&m_npc), STARTING_POINTS + NPC_WHOLE_MANA_PER_PASS)
        << describe_gain(NPC_HOURLY_MANA_GAIN, ROLL_ADDS_NOTHING);
    expect_every_queued_roll_drawn();
}

TEST_F(FastUpdateRegenTest, APassGivesAnNpcItsHourlyGainsDividedByTheFastUpdateRate)
{
    ASSERT_FLOAT_EQ(hit_gain(&m_npc), NPC_HOURLY_HIT_GAIN);
    ASSERT_FLOAT_EQ(move_gain(&m_npc), NPC_HOURLY_MOVE_GAIN);
    ASSERT_FLOAT_EQ(mana_gain(&m_npc), NPC_HOURLY_MANA_GAIN);
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();

    EXPECT_EQ(GET_HIT(&m_npc), STARTING_POINTS + NPC_WHOLE_HIT_POINTS_PER_PASS)
        << describe_gain(NPC_HOURLY_HIT_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_MOVE(&m_npc), STARTING_POINTS + NPC_WHOLE_MOVES_PER_PASS)
        << describe_gain(NPC_HOURLY_MOVE_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_MANA(&m_npc), STARTING_POINTS + NPC_WHOLE_MANA_PER_PASS)
        << describe_gain(NPC_HOURLY_MANA_GAIN, ROLL_ADDS_NOTHING);
    expect_every_queued_roll_drawn();
}

// Each pool has its own maximum and gains more than one point, so only its own cap stops it.
TEST_F(FastUpdateRegenTest, EachPoolStopsAtItsOwnMaximum)
{
    constexpr int MAXIMUM_HIT_POINTS = 400;
    constexpr int MAXIMUM_MOVES = 450;
    constexpr int MAXIMUM_MANA = 500;
    m_player.abilities.hit = MAXIMUM_HIT_POINTS;
    m_player.abilities.move = MAXIMUM_MOVES;
    m_player.abilities.mana = MAXIMUM_MANA;
    set_pools(m_player, MAXIMUM_HIT_POINTS - 1, MAXIMUM_MOVES - 1, MAXIMUM_MANA - 1);
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();

    EXPECT_EQ(GET_HIT(&m_player), MAXIMUM_HIT_POINTS);
    EXPECT_EQ(GET_MOVE(&m_player), MAXIMUM_MOVES);
    EXPECT_EQ(GET_MANA(&m_player), MAXIMUM_MANA);
    expect_every_queued_roll_drawn();
}

TEST_F(FastUpdateRegenTest, APoolAboveItsMaximumDropsToTheMaximum)
{
    constexpr int ABOVE_MAXIMUM = MAXIMUM_POINTS + 100;
    set_pools(m_player, ABOVE_MAXIMUM, ABOVE_MAXIMUM, ABOVE_MAXIMUM);
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();

    EXPECT_EQ(GET_HIT(&m_player), MAXIMUM_POINTS);
    EXPECT_EQ(GET_MOVE(&m_player), MAXIMUM_POINTS);
    EXPECT_EQ(GET_MANA(&m_player), MAXIMUM_POINTS);
    expect_every_queued_roll_drawn();
}

TEST_F(FastUpdateRegenTest, NegativeRegenDrainsAFullPool)
{
    constexpr float HOURLY_MANA_GAIN = -60.0f; // exactly -3 per pass
    m_player.points.mana_regen = static_cast<int>(HOURLY_MANA_GAIN - PLAYER_FORMULA_MANA_GAIN);
    set_pools(m_player, STARTING_POINTS, STARTING_POINTS, MAXIMUM_POINTS);
    ASSERT_FLOAT_EQ(mana_gain(&m_player), HOURLY_MANA_GAIN);
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();

    EXPECT_EQ(GET_MANA(&m_player), MAXIMUM_POINTS - 3)
        << describe_gain(HOURLY_MANA_GAIN, ROLL_ADDS_NOTHING);
    expect_every_queued_roll_drawn();
}

TEST_F(FastUpdateRegenTest, ARollBelowANegativeGainsFractionTakesOneMorePoint)
{
    constexpr float HOURLY_MANA_GAIN = -50.0f; // -2.5 per pass
    m_player.points.mana_regen = static_cast<int>(HOURLY_MANA_GAIN - PLAYER_FORMULA_MANA_GAIN);
    ASSERT_FLOAT_EQ(mana_gain(&m_player), HOURLY_MANA_GAIN);
    queue_character_rolls(ROLL_ADDS_NOTHING, ROLL_ADDS_NOTHING, ROLL_ADDS_ONE_POINT);
    queue_rolls(ROLL_ADDS_NOTHING, 1);

    regeneration_update();

    EXPECT_EQ(GET_MANA(&m_player), STARTING_POINTS - 3)
        << describe_gain(HOURLY_MANA_GAIN, ROLL_ADDS_ONE_POINT);
    expect_every_queued_roll_drawn();
}

// Pins the current behaviour: a negative per-pass gain between -1 and 0 truncates to 0, and a
// roll below its fraction then adds a point instead of taking one.
TEST_F(FastUpdateRegenTest, LegacyARollBelowASmallNegativeGainsFractionAddsAPoint)
{
    constexpr float HOURLY_MANA_GAIN = -10.0f; // -0.5 per pass
    m_player.points.mana_regen = static_cast<int>(HOURLY_MANA_GAIN - PLAYER_FORMULA_MANA_GAIN);
    ASSERT_FLOAT_EQ(mana_gain(&m_player), HOURLY_MANA_GAIN);
    queue_character_rolls(ROLL_ADDS_NOTHING, ROLL_ADDS_NOTHING, ROLL_ADDS_ONE_POINT);
    queue_rolls(ROLL_ADDS_NOTHING, 1);

    regeneration_update();

    EXPECT_EQ(GET_MANA(&m_player), STARTING_POINTS + 1)
        << describe_gain(HOURLY_MANA_GAIN, ROLL_ADDS_ONE_POINT);
    expect_every_queued_roll_drawn();
}

TEST_F(FastUpdateRegenTest, MovesNeverDropBelowZero)
{
    constexpr int DRAINING_MOVE_REGEN = -2000;
    m_player.points.move_regen = DRAINING_MOVE_REGEN;
    set_pools(m_player, STARTING_POINTS, 1, STARTING_POINTS);
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();

    EXPECT_EQ(GET_MOVE(&m_player), 0)
        << describe_gain(PLAYER_FORMULA_MOVE_GAIN + DRAINING_MOVE_REGEN, ROLL_ADDS_NOTHING);
    expect_every_queued_roll_drawn();
}

TEST_F(FastUpdateRegenTest, TheBreathingAndFameWarChecksRunForEveryCharacter)
{
    add_check_markers(m_player, m_markers[0]);
    add_check_markers(m_npc, m_markers[1]);

    fast_update();

    EXPECT_EQ(m_markers[0].modifier, ASPHYXIATION_MARKER - 1) << "check_breathing() on the player";
    EXPECT_EQ(m_player.player.ranking, 0) << "do_fame_war_bonuses() on the unranked player";
    EXPECT_EQ(m_markers[1].modifier, ASPHYXIATION_MARKER - 1) << "check_breathing() on the NPC";
}

// A pass that leaves a character on exactly 0 hit points does not kill it.
TEST_F(FastUpdateRegenTest, RegenThatLeavesZeroHitPointsDoesNotKill)
{
    constexpr float HOURLY_HIT_GAIN = -100.0f; // exactly -5 per pass
    build_dying_npc();
    m_dying_npc.points.health_regen = static_cast<int>(HOURLY_HIT_GAIN - NPC_HOURLY_HIT_GAIN);
    GET_HIT(&m_dying_npc) = 5;
    ASSERT_FLOAT_EQ(hit_gain(&m_dying_npc), HOURLY_HIT_GAIN);
    register_characters({ &m_dying_npc, &m_player });
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();

    EXPECT_EQ(g_death_special_calls, 0);
    EXPECT_EQ(GET_HIT(&m_dying_npc), 0) << describe_gain(HOURLY_HIT_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS + PLAYER_WHOLE_HIT_POINTS_PER_PASS)
        << "the pass should go on to the next character";
    expect_every_queued_roll_drawn();
}

// Only negative regen kills: a character below zero that is healing survives the pass.
TEST_F(FastUpdateRegenTest, ACharacterBelowZeroThatIsHealingDoesNotDie)
{
    build_dying_npc();
    m_dying_npc.points.health_regen = 0;
    GET_HIT(&m_dying_npc) = -10;
    register_characters({ &m_dying_npc, &m_player });
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();

    EXPECT_EQ(g_death_special_calls, 0);
    EXPECT_EQ(GET_HIT(&m_dying_npc), -10 + NPC_WHOLE_HIT_POINTS_PER_PASS)
        << describe_gain(NPC_HOURLY_HIT_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS + PLAYER_WHOLE_HIT_POINTS_PER_PASS)
        << "the pass should go on to the next character";
    expect_every_queued_roll_drawn();
}

// The pass stops at a regen death, before the dead character's moves and mana, so characters
// after it in character_list do not regenerate on that pass.
TEST_F(FastUpdateRegenTest, ARegenDeathStopsRegenerationBeforeTheNextCharacter)
{
    build_dying_npc();
    register_characters({ &m_player, &m_dying_npc, &m_second_player });
    // The dying NPC draws all three of its rolls before its death; the player after it draws none.
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();

    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS + PLAYER_WHOLE_HIT_POINTS_PER_PASS)
        << "the character before the death regenerates";

    EXPECT_EQ(g_death_special_calls, 1) << "the NPC should have died of its regen";
    EXPECT_LT(GET_HIT(&m_dying_npc), 0);
    EXPECT_EQ(GET_MOVE(&m_dying_npc), STARTING_POINTS)
        << "the death comes before moves would gain " << NPC_WHOLE_MOVES_PER_PASS << " ("
        << describe_gain(NPC_HOURLY_MOVE_GAIN, ROLL_ADDS_NOTHING) << ")";
    EXPECT_EQ(GET_MANA(&m_dying_npc), STARTING_POINTS)
        << "the death comes before mana would gain " << NPC_WHOLE_MANA_PER_PASS << " ("
        << describe_gain(NPC_HOURLY_MANA_GAIN, ROLL_ADDS_NOTHING) << ")";

    EXPECT_EQ(GET_HIT(&m_second_player), STARTING_POINTS)
        << "the pass should end at the death, before the hit point gain of "
        << PLAYER_WHOLE_HIT_POINTS_PER_PASS;
    EXPECT_EQ(GET_MOVE(&m_second_player), STARTING_POINTS);
    EXPECT_EQ(GET_MANA(&m_second_player), STARTING_POINTS);
    expect_every_queued_roll_drawn();
}

// Regeneration then the checks: a regen death stops regeneration, but the characters after it
// in character_list still get the checks. The dead NPC's own checks are not asserted: the live
// game extracts it, where the test's death special keeps it in the list.
TEST_F(FastUpdateRegenTest, TheChecksStillReachCharactersAfterARegenDeath)
{
    build_dying_npc();
    add_check_markers(m_player, m_markers[0]);
    add_check_markers(m_second_player, m_markers[2]);
    register_characters({ &m_player, &m_dying_npc, &m_second_player });
    // The dying NPC draws all three of its rolls before its death; the player after it draws none.
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();
    fast_update();

    ASSERT_EQ(g_death_special_calls, 1) << "the NPC should have died of its regen";
    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS + PLAYER_WHOLE_HIT_POINTS_PER_PASS)
        << "the character before the death regenerates";
    EXPECT_EQ(GET_HIT(&m_second_player), STARTING_POINTS)
        << "the character after the death should not regenerate";
    EXPECT_EQ(GET_MOVE(&m_second_player), STARTING_POINTS);
    EXPECT_EQ(GET_MANA(&m_second_player), STARTING_POINTS);
    EXPECT_EQ(m_markers[0].modifier, ASPHYXIATION_MARKER - 1)
        << "check_breathing() on the first player";
    EXPECT_EQ(m_player.player.ranking, 0) << "do_fame_war_bonuses() on the first player";
    EXPECT_EQ(m_markers[2].modifier, ASPHYXIATION_MARKER - 1)
        << "check_breathing() on the player after the death";
    EXPECT_EQ(m_second_player.player.ranking, 0)
        << "do_fame_war_bonuses() on the player after the death";
    expect_every_queued_roll_drawn();
}

// A death at the end of character_list comes after every other character has regenerated.
TEST_F(FastUpdateRegenTest, ARegenDeathLastInTheListComesAfterEveryOtherCharacterRegenerates)
{
    build_dying_npc();
    register_characters({ &m_player, &m_npc, &m_dying_npc });
    queue_rolls(ROLL_ADDS_NOTHING, 3);

    regeneration_update();

    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS + PLAYER_WHOLE_HIT_POINTS_PER_PASS)
        << describe_gain(PLAYER_HOURLY_HIT_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_MANA(&m_player), STARTING_POINTS + PLAYER_WHOLE_MANA_PER_PASS)
        << describe_gain(PLAYER_HOURLY_MANA_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_HIT(&m_npc), STARTING_POINTS + NPC_WHOLE_HIT_POINTS_PER_PASS)
        << describe_gain(NPC_HOURLY_HIT_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(GET_MANA(&m_npc), STARTING_POINTS + NPC_WHOLE_MANA_PER_PASS)
        << describe_gain(NPC_HOURLY_MANA_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(g_death_special_calls, 1) << "the last NPC should have died of its regen";
    EXPECT_LT(GET_HIT(&m_dying_npc), 0);
    expect_every_queued_roll_drawn();
}

TEST_F(FastUpdateRegenTest, RegenerationUpdateRunsNoChecks)
{
    add_check_markers(m_player, m_markers[0]);
    add_check_markers(m_npc, m_markers[1]);
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    regeneration_update();

    EXPECT_EQ(m_markers[0].modifier, ASPHYXIATION_MARKER) << "no check_breathing() on the player";
    EXPECT_EQ(m_player.player.ranking, RANKING_MARKER) << "no do_fame_war_bonuses() on the player";
    EXPECT_EQ(m_markers[1].modifier, ASPHYXIATION_MARKER) << "no check_breathing() on the NPC";
    expect_every_queued_roll_drawn();
}

TEST_F(FastUpdateRegenTest, FastUpdateLeavesEveryPoolUnchanged)
{
    build_dying_npc();
    register_characters({ &m_player, &m_dying_npc, &m_npc });

    fast_update();

    for (const char_data* character : { &m_player, &m_dying_npc, &m_npc }) {
        EXPECT_EQ(GET_HIT(character), STARTING_POINTS) << GET_NAME(character);
        EXPECT_EQ(GET_MOVE(character), STARTING_POINTS) << GET_NAME(character);
        EXPECT_EQ(GET_MANA(character), STARTING_POINTS) << GET_NAME(character);
    }
    EXPECT_EQ(g_death_special_calls, 0) << "fast_update() must not apply lethal regen";
}

TEST_F(FastUpdateRegenTest, FastUpdateDrawsNoRandomNumbers)
{
    for (const double roll : UNTOUCHED_ROLLS) {
        push_test_random_value(roll);
    }

    fast_update();

    // In the test build number() returns queued rolls first, so all three must still be queued.
    for (const double roll : UNTOUCHED_ROLLS) {
        const double next_roll = number();
        EXPECT_DOUBLE_EQ(next_roll, roll) << "the pass should have drawn none of the queued rolls";
    }
}

// The room is lit so only the race gate keeps the Power of Arda off the human.
TEST_F(FastUpdateRegenTest, FastUpdateGivesAHumanNoPowerOfArdaInSunlight)
{
    light_the_room();
    ASSERT_EQ(get_sun_level(REGEN_ROOM), SUNLIT_FIELD_SUN_LEVEL);

    fast_update();

    EXPECT_EQ(affected_by_spell(&m_player, SPELL_ARDA), nullptr)
        << "Expected no Power of Arda on a human.";
}

TEST_F(FastUpdateRegenTest, FastUpdateGivesAnOrcThePowerOfArdaInSunlight)
{
    light_the_room();
    ASSERT_EQ(get_sun_level(REGEN_ROOM), SUNLIT_FIELD_SUN_LEVEL);
    m_npc.player.race = RACE_ORC;

    fast_update();

    const affected_type* arda = affected_by_spell(&m_npc, SPELL_ARDA);
    ASSERT_NE(arda, nullptr) << "Expected the Power of Arda on an orc in sunlight.";
    EXPECT_EQ(arda->modifier, SUNLIT_FIELD_SUN_LEVEL);
}

} // namespace
