// Pins which character updates one GameHeartbeat pass runs for each regeneration and
// fast-update flag of its WorldPass, and that regeneration runs before the fast update's checks.

#include "../game_heartbeat.h"
#include "../game_tick_schedule.h"
#include "regeneration_fixture.h"

#include <gtest/gtest.h>

#include <iterator>
#include <memory>

extern struct char_data* combat_list;
extern struct descriptor_data* descriptor_list;

namespace {

using namespace regen_test;

// A schedule that returns the same pass, chosen by the test, every time it is asked.
class FixedPassSchedule : public GameTickSchedule {
public:
    [[nodiscard]] WorldPass next_pass() override
    {
        return world_pass;
    }

    // The pass every next_pass() call returns; no world update until a test sets one.
    WorldPass world_pass;
};

// The regeneration fixture's characters, with empty descriptor and combat lists for the
// heartbeat's housekeeping, and a heartbeat running one pass of the test's choosing.
class HeartbeatWorldUpdateTest : public RegenerationFixture {
protected:
    void SetUp() override
    {
        RegenerationFixture::SetUp();
        if (HasFatalFailure()) {
            return;
        }
        m_saved_descriptor_list = descriptor_list;
        descriptor_list = nullptr;
        m_saved_combat_list = combat_list;
        combat_list = nullptr;
        m_lists_saved = true;
    }

    void TearDown() override
    {
        if (m_lists_saved) {
            descriptor_list = m_saved_descriptor_list;
            combat_list = m_saved_combat_list;
            set_current_time_phase(0);
        }
        RegenerationFixture::TearDown();
    }

    // Runs one heartbeat pass whose WorldPass sets only the two given flags.
    static void run_one_pass(bool run_regeneration, bool run_fast_update)
    {
        const std::shared_ptr<FixedPassSchedule> schedule = std::make_shared<FixedPassSchedule>();
        schedule->world_pass.run_regeneration = run_regeneration;
        schedule->world_pass.run_fast_update = run_fast_update;
        GameHeartbeat heartbeat(schedule);
        heartbeat.run_pass();
    }

    // Queues UNTOUCHED_ROLLS, for a test that expects the pass to draw none of them.
    static void queue_untouched_rolls()
    {
        for (const double roll : UNTOUCHED_ROLLS) {
            push_test_random_value(roll);
        }
    }

    descriptor_data* m_saved_descriptor_list = nullptr; // restored in TearDown()
    char_data* m_saved_combat_list = nullptr; // restored in TearDown()
    bool m_lists_saved = false; // true once SetUp() has saved the two lists
};

TEST_F(HeartbeatWorldUpdateTest, ARegenerationOnlyPassRegeneratesAndRunsNoChecks)
{
    add_check_markers(m_player, m_markers[0]);
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    run_one_pass(true, false);

    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS + PLAYER_WHOLE_HIT_POINTS_PER_PASS)
        << describe_gain(PLAYER_HOURLY_HIT_GAIN, ROLL_ADDS_NOTHING);
    EXPECT_EQ(m_markers[0].modifier, ASPHYXIATION_MARKER) << "no check_breathing() on the player";
    EXPECT_EQ(m_player.player.ranking, RANKING_MARKER) << "no do_fame_war_bonuses() on the player";
    expect_every_queued_roll_drawn();
}

TEST_F(HeartbeatWorldUpdateTest, AFastUpdateOnlyPassRunsTheChecksWithoutRegenerating)
{
    add_check_markers(m_player, m_markers[0]);
    queue_untouched_rolls();

    run_one_pass(false, true);

    EXPECT_EQ(m_markers[0].modifier, ASPHYXIATION_MARKER - 1) << "check_breathing() on the player";
    EXPECT_EQ(m_player.player.ranking, 0) << "do_fame_war_bonuses() on the player";
    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS);
    EXPECT_EQ(GET_MOVE(&m_player), STARTING_POINTS);
    EXPECT_EQ(GET_MANA(&m_player), STARTING_POINTS);
    EXPECT_EQ(test_random_values_remaining(), std::size(UNTOUCHED_ROLLS))
        << "Expected the pass to draw none of the queued rolls.";
}

TEST_F(HeartbeatWorldUpdateTest, APassWithNeitherFlagChangesNoCharacter)
{
    add_check_markers(m_player, m_markers[0]);
    queue_untouched_rolls();

    run_one_pass(false, false);

    EXPECT_EQ(m_markers[0].modifier, ASPHYXIATION_MARKER) << "no check_breathing() on the player";
    EXPECT_EQ(m_player.player.ranking, RANKING_MARKER) << "no do_fame_war_bonuses() on the player";
    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS);
    EXPECT_EQ(GET_MOVE(&m_player), STARTING_POINTS);
    EXPECT_EQ(GET_MANA(&m_player), STARTING_POINTS);
    EXPECT_EQ(test_random_values_remaining(), std::size(UNTOUCHED_ROLLS))
        << "Expected the pass to draw none of the queued rolls.";
}

// The NPC dies of its regen inside regeneration_update(); its death special reads the player's
// marker then, which check_breathing() has not yet lowered if regeneration runs first.
TEST_F(HeartbeatWorldUpdateTest, RegenerationRunsBeforeTheChecks)
{
    build_dying_npc();
    add_check_markers(m_player, m_markers[0]);
    register_characters({ &m_player, &m_dying_npc });
    g_marker_watched_at_death = &m_markers[0];
    queue_rolls(ROLL_ADDS_NOTHING, 2);

    run_one_pass(true, true);

    ASSERT_EQ(g_death_special_calls, 1) << "the NPC should have died of its regen";
    EXPECT_EQ(g_watched_modifier_at_death, ASPHYXIATION_MARKER)
        << "Expected the death, and so regeneration, before check_breathing() on the player.";
    EXPECT_EQ(m_markers[0].modifier, ASPHYXIATION_MARKER - 1) << "check_breathing() on the player";
    EXPECT_EQ(GET_HIT(&m_player), STARTING_POINTS + PLAYER_WHOLE_HIT_POINTS_PER_PASS)
        << describe_gain(PLAYER_HOURLY_HIT_GAIN, ROLL_ADDS_NOTHING);
    expect_every_queued_roll_drawn();
}

} // namespace
