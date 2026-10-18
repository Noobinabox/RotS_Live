// Fixes what one fast_update() pass does to hit points, moves and mana: each character gains a
// FAST_UPDATE_RATE-th of its hourly gain, the fraction is settled by one random roll per point
// pool, every pool is capped at its maximum, and a death from negative regen ends the pass.

#include "../db.h"
#include "../interpre.h"
#include "../limits.h"
#include "../professions.h"
#include "../structs.h"
#include "../utils.h"
#include "test_random_utils.h"

#include <gtest/gtest.h>

extern room_data world;
extern char_data* character_list;
extern char_data* waiting_list;
extern index_data* mob_index;
extern int top_of_mobt;
void clear_char(char_data* ch, int mode);
void fast_update(); // limits.cpp: not declared in limits.h

namespace {

// A room no other suite in the binary uses, below the 1024 that gtest_main.cpp allocates.
constexpr int kRegenRoom = 1012;

// A roll at or above any fraction a gain leaves, so a pass adds only the whole points.
constexpr double kRollAddsNothing = 0.999999;
// A roll below any non-zero fraction, so a pass adds one point more than the whole points.
constexpr double kRollAddsOnePoint = 0.0;
// fast_update() rolls once for each of hit points, moves and mana, in that order.
constexpr int kRollsPerCharacter = 3;

// The sleeping player's hourly gains, from limits.cpp's formulas and the stats set in SetUp():
// hit points (8 + 18 con / 2 + 30 warrior / 3 + 15 ranger / 5) * 1.5 sleeping = 45,
// moves (7 + (18 con + 18 dex) / 2 + 15 ranger / 6) * 1.75 sleeping = 48.125,
// mana (8 + 20 int / 2 + 10 wil / 5 + 20 mage / 5 + 10 cleric / 5) * 2 sleeping = 52.
constexpr float kPlayerHourlyHitGain = 45.0f;
constexpr float kPlayerHourlyMoveGain = 48.125f;
constexpr float kPlayerHourlyManaGain = 52.0f;
// Each of the player's gains is 2 whole points per pass with a fraction left over.
constexpr int kPlayerWholePointsPerPass = 2;

// The level-30 NPC's hourly gains: an NPC out of combat gains its level * 1.5 hit points and
// mana, and 7 + (20 con + 20 dex) / 2 + 30 level / 6 + 80 default knowledge / 40 - 5 = 29 moves.
constexpr float kNpcHourlyHitGain = 45.0f;
constexpr float kNpcHourlyMoveGain = 29.0f;
constexpr float kNpcHourlyManaGain = 45.0f;
constexpr int kNpcWholeHitPointsPerPass = 2;
constexpr int kNpcWholeMovesPerPass = 1;
constexpr int kNpcWholeManaPerPass = 2;

// Every pool starts here, below its maximum, unless a test sets it otherwise.
constexpr int kStartingPoints = 100;
constexpr int kMaximumPoints = 500;

// A flat health regen far below any natural gain, so one pass takes a character from
// kStartingPoints to below zero.
constexpr int kLethalHourlyHealthRegen = -1000 * FAST_UPDATE_RATE;

// How many times dying_npc_special() has been asked about a death since the fixture's SetUp().
int g_death_special_calls = 0;

// A mob special that claims every death it is asked about, so raw_kill() returns before it
// extracts and frees the NPC and the test can still read the NPC after the pass.
int dying_npc_special(char_data* /*host*/, char_data* /*character*/, int /*cmd*/, char* /*argument*/,
    int callflag, waiting_type* /*wait_data*/)
{
    if (callflag == SPECIAL_DEATH) {
        ++g_death_special_calls;
        return TRUE;
    }
    return FALSE;
}

// One sleeping human player and one level-30 NPC standing in kRegenRoom, both registered in
// character_list with known stats, and the global lists fast_update() and raw_kill() walk
// isolated from the rest of the binary.
class FastUpdateRegenTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_saved_character_list = character_list;
        m_saved_waiting_list = waiting_list;
        m_saved_mob_index = mob_index;
        m_saved_top_of_mobt = top_of_mobt;
        m_saved_room_people = world[kRegenRoom].people;
        m_saved_room_sector = world[kRegenRoom].sector_type;

        // raw_kill() walks waiting_list; earlier suites can leave stale entries on it.
        waiting_list = nullptr;
        m_mob_index[0].func = dying_npc_special;
        mob_index = m_mob_index;
        top_of_mobt = 0;
        world[kRegenRoom].sector_type = SECT_INSIDE;
        world[kRegenRoom].people = nullptr;
        clear_test_random_values();
        g_death_special_calls = 0;

        clear_char(&m_player, MOB_VOID);
        m_player.player.name = m_player_name;
        m_player.player.level = 20;
        m_player.player.race = RACE_HUMAN;
        m_player.player_index = -1;
        m_player.specials.position = POSITION_SLEEPING;
        m_player.specials2.conditions[FULL] = 24;
        m_player.specials2.conditions[THIRST] = 24;
        m_player.tmpabilities.con = 18;
        m_player.tmpabilities.dex = 18;
        m_player.tmpabilities.intel = 20;
        m_player.tmpabilities.wil = 10;
        m_player.profs->prof_level[PROF_WARRIOR] = 30;
        m_player.profs->prof_level[PROF_RANGER] = 15;
        m_player.profs->prof_level[PROF_MAGE] = 20;
        m_player.profs->prof_level[PROF_CLERIC] = 10;
        set_points(m_player, kStartingPoints, kMaximumPoints);

        clear_char(&m_npc, MOB_ISNPC);
        SET_BIT(MOB_FLAGS(&m_npc), MOB_ISNPC);
        m_npc.nr = -1;
        m_npc.player.name = m_npc_name;
        m_npc.player.level = 30;
        m_npc.specials2.conditions[FULL] = -1;
        m_npc.specials2.conditions[THIRST] = -1;
        m_npc.tmpabilities.con = 20;
        m_npc.tmpabilities.dex = 20;
        set_points(m_npc, kStartingPoints, kMaximumPoints);

        place_in_room(m_player);
        place_in_room(m_npc);
        register_characters(&m_player, &m_npc);
    }

    void TearDown() override
    {
        clear_test_random_values();
        for (char_data* character : { &m_player, &m_npc, &m_dying_npc }) {
            RELEASE(character->profs);
            RELEASE(character->skills);
            RELEASE(character->knowledge);
        }
        character_list = m_saved_character_list;
        waiting_list = m_saved_waiting_list;
        mob_index = m_saved_mob_index;
        top_of_mobt = m_saved_top_of_mobt;
        world[kRegenRoom].people = m_saved_room_people;
        world[kRegenRoom].sector_type = m_saved_room_sector;
    }

    static void set_points(char_data& character, int current_points, int maximum_points)
    {
        character.tmpabilities.hit = current_points;
        character.tmpabilities.move = current_points;
        character.tmpabilities.mana = current_points;
        character.abilities.hit = maximum_points;
        character.abilities.move = maximum_points;
        character.abilities.mana = maximum_points;
    }

    static void place_in_room(char_data& character)
    {
        character.in_room = kRegenRoom;
        character.next_in_room = world[kRegenRoom].people;
        world[kRegenRoom].people = &character;
    }

    // Makes `first` then `second` the whole of character_list, in pass order.
    static void register_characters(char_data* first, char_data* second)
    {
        character_list = first;
        first->next = second;
        second->next = nullptr;
    }

    // Queues `roll` for every random roll fast_update() makes for `character_count` characters.
    static void queue_rolls(double roll, int character_count)
    {
        for (int roll_index = 0; roll_index < character_count * kRollsPerCharacter; ++roll_index) {
            push_test_random_value(roll);
        }
    }

    // Turns m_dying_npc into an NPC whose death special keeps it alive, losing far more hit
    // points per pass than it has.
    void build_dying_npc()
    {
        clear_char(&m_dying_npc, MOB_ISNPC);
        SET_BIT(MOB_FLAGS(&m_dying_npc), MOB_ISNPC);
        SET_BIT(MOB_FLAGS(&m_dying_npc), MOB_SPEC);
        m_dying_npc.nr = 0;
        m_dying_npc.player.name = m_dying_npc_name;
        m_dying_npc.player.level = 30;
        m_dying_npc.specials2.conditions[FULL] = -1;
        m_dying_npc.specials2.conditions[THIRST] = -1;
        m_dying_npc.points.health_regen = kLethalHourlyHealthRegen;
        set_points(m_dying_npc, kStartingPoints, kMaximumPoints);
        place_in_room(m_dying_npc);
    }

    char_data m_player {}; // the sleeping human player; first in character_list unless a test reorders it
    char_data m_npc {}; // the level-30 NPC without a special; second in character_list
    char_data m_dying_npc {}; // built only by build_dying_npc(); otherwise unregistered
    char m_player_name[16] = "Sleeper"; // m_player's name storage
    char m_npc_name[16] = "a test orc"; // m_npc's name storage
    char m_dying_npc_name[24] = "a withering orc"; // m_dying_npc's name storage
    index_data m_mob_index[1] {}; // mob_index for the test: entry 0 is m_dying_npc's death special

    char_data* m_saved_character_list = nullptr; // restored in TearDown()
    char_data* m_saved_waiting_list = nullptr; // restored in TearDown()
    index_data* m_saved_mob_index = nullptr; // restored in TearDown()
    int m_saved_top_of_mobt = 0; // restored in TearDown()
    char_data* m_saved_room_people = nullptr; // kRegenRoom's occupants, restored in TearDown()
    int m_saved_room_sector = 0; // kRegenRoom's sector, restored in TearDown()
};

TEST_F(FastUpdateRegenTest, APassGivesAPlayerItsHourlyGainsDividedByTheFastUpdateRate)
{
    ASSERT_FLOAT_EQ(hit_gain(&m_player), kPlayerHourlyHitGain);
    ASSERT_FLOAT_EQ(move_gain(&m_player), kPlayerHourlyMoveGain);
    ASSERT_FLOAT_EQ(mana_gain(&m_player), kPlayerHourlyManaGain);
    ASSERT_EQ(static_cast<int>(kPlayerHourlyHitGain / FAST_UPDATE_RATE), kPlayerWholePointsPerPass);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_player), kStartingPoints + kPlayerWholePointsPerPass);
    EXPECT_EQ(GET_MOVE(&m_player), kStartingPoints + kPlayerWholePointsPerPass);
    EXPECT_EQ(GET_MANA(&m_player), kStartingPoints + kPlayerWholePointsPerPass);
}

TEST_F(FastUpdateRegenTest, ARollBelowAGainsFractionAddsOneMorePoint)
{
    queue_rolls(kRollAddsOnePoint, 2);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_player), kStartingPoints + kPlayerWholePointsPerPass + 1);
    EXPECT_EQ(GET_MOVE(&m_player), kStartingPoints + kPlayerWholePointsPerPass + 1);
    EXPECT_EQ(GET_MANA(&m_player), kStartingPoints + kPlayerWholePointsPerPass + 1);
}

TEST_F(FastUpdateRegenTest, APassGivesAnNpcItsHourlyGainsDividedByTheFastUpdateRate)
{
    ASSERT_FLOAT_EQ(hit_gain(&m_npc), kNpcHourlyHitGain);
    ASSERT_FLOAT_EQ(move_gain(&m_npc), kNpcHourlyMoveGain);
    ASSERT_FLOAT_EQ(mana_gain(&m_npc), kNpcHourlyManaGain);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_npc), kStartingPoints + kNpcWholeHitPointsPerPass);
    EXPECT_EQ(GET_MOVE(&m_npc), kStartingPoints + kNpcWholeMovesPerPass);
    EXPECT_EQ(GET_MANA(&m_npc), kStartingPoints + kNpcWholeManaPerPass);
}

// The player gains more than one point in each pool, so only the cap stops it at the maximum.
TEST_F(FastUpdateRegenTest, EachPoolStopsAtItsMaximum)
{
    set_points(m_player, kMaximumPoints - 1, kMaximumPoints);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_player), kMaximumPoints);
    EXPECT_EQ(GET_MOVE(&m_player), kMaximumPoints);
    EXPECT_EQ(GET_MANA(&m_player), kMaximumPoints);
}

// Fixes the current behaviour: the pass stops at a regen death, so characters after the dead
// one in character_list miss that pass's regeneration.
TEST_F(FastUpdateRegenTest, LegacyARegenDeathEndsThePassBeforeTheNextCharacter)
{
    build_dying_npc();
    register_characters(&m_dying_npc, &m_player);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(g_death_special_calls, 1) << "the first character should have died of its regen";
    EXPECT_LT(GET_HIT(&m_dying_npc), 0);
    EXPECT_EQ(GET_HIT(&m_player), kStartingPoints) << "the pass should end at the death";
    EXPECT_EQ(GET_MOVE(&m_player), kStartingPoints);
    EXPECT_EQ(GET_MANA(&m_player), kStartingPoints);
}

} // namespace
