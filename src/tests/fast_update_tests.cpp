// Fixes what one fast_update() pass does to hit points, moves and mana: each character gains a
// FAST_UPDATE_RATE-th of its hourly gain, the fraction is settled by one random roll per point
// pool, every pool is capped at its maximum, moves never drop below zero, the breathing and fame
// war checks run after each character's regen, and a death from negative regen ends the pass.

#include "../db.h"
#include "../interpre.h"
#include "../limits.h"
#include "../professions.h"
#include "../spells.h"
#include "../structs.h"
#include "../utils.h"
#include "test_random_utils.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <sstream>
#include <string>

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
// Above the player's hit (0.25) and mana (about 0.1) fractions and below its move fraction
// (0.40625), so it adds a point only when it is drawn for moves.
constexpr double kRollAddsOnlyToPlayerMoves = 0.3;

// The sleeping player's hourly gains from limits.cpp's formulas and the stats set in
// build_player():
// hit points (8 + 18 con / 2 + 30 warrior / 3 + 15 ranger / 5) * 1.5 sleeping = 45,
// moves (7 + (18 con + 18 dex) / 2 + 15 ranger / 6) * 1.75 sleeping = 48.125,
// mana (8 + 20 int / 2 + 10 wil / 5 + 20 mage / 5 + 10 cleric / 5) * 2 sleeping = 52.
// The flat move and mana regen are added after the formula.
constexpr float kPlayerHourlyHitGain = 45.0f;
constexpr float kPlayerFormulaMoveGain = 48.125f;
constexpr float kPlayerFormulaManaGain = 52.0f;
constexpr int kPlayerFlatMoveRegen = 40;
constexpr int kPlayerFlatManaRegen = 10;
constexpr float kPlayerHourlyMoveGain = kPlayerFormulaMoveGain + kPlayerFlatMoveRegen; // 88.125
constexpr float kPlayerHourlyManaGain = kPlayerFormulaManaGain + kPlayerFlatManaRegen; // 62
// The whole points per pass, different for each pool so a pool given another's value is caught.
constexpr int kPlayerWholeHitPointsPerPass = 2; // 2.25
constexpr int kPlayerWholeMovesPerPass = 4; // 4.40625
constexpr int kPlayerWholeManaPerPass = 3; // about 3.1

// The level-30 NPC's hourly gains: an NPC out of combat gains its level * 1.5 hit points and
// mana, and 7 + (20 con + 20 dex) / 2 + 30 level / 6 + 80 default knowledge / 40 - 5 = 29 moves.
constexpr float kNpcHourlyHitGain = 45.0f;
constexpr float kNpcHourlyMoveGain = 29.0f;
constexpr float kNpcHourlyManaGain = 45.0f;
constexpr int kNpcWholeHitPointsPerPass = 2; // 2.25
constexpr int kNpcWholeMovesPerPass = 1; // 1.45
constexpr int kNpcWholeManaPerPass = 2; // 2.25

// Every pool starts here, below its maximum, unless a test sets it otherwise.
constexpr int kStartingPoints = 100;
constexpr int kMaximumPoints = 500;

// A flat health regen far below any natural gain, so one pass takes a character from
// kStartingPoints to below zero.
constexpr int kLethalHourlyHealthRegen = -1000 * FAST_UPDATE_RATE;

// check_breathing() lowers an asphyxiation affect's modifier by one each pass a character can
// breathe; above 1, so a pass leaves the affect in place.
constexpr int kAsphyxiationMarker = 5;
// do_fame_war_bonuses() sets an unranked player's ranking to 0; any other value shows whether
// it ran.
constexpr int kRankingMarker = 7;

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

// Describes how a pass's change was derived, for failure messages.
std::string describe_gain(float hourly_gain, double roll)
{
    std::ostringstream description;
    description << "hourly gain " << hourly_gain << " / FAST_UPDATE_RATE " << FAST_UPDATE_RATE
                << ", roll " << roll;
    return description.str();
}

// Two sleeping human players and one level-30 NPC standing in kRegenRoom with known stats, the
// first player and the NPC registered in character_list, and the global lists fast_update() and
// raw_kill() walk isolated from the rest of the binary.
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

        build_player(m_player, m_player_name);
        build_player(m_second_player, m_second_player_name);

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
        place_in_room(m_npc);

        register_characters({ &m_player, &m_npc });
    }

    void TearDown() override
    {
        clear_test_random_values();
        for (char_data* character : { &m_player, &m_second_player, &m_npc, &m_dying_npc }) {
            // The markers are fixture members, not heap affects.
            character->affected = nullptr;
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

    // Makes `character` the sleeping level-20 human player whose gains the kPlayer constants
    // describe, standing in kRegenRoom but not registered in character_list.
    void build_player(char_data& character, char* name)
    {
        clear_char(&character, MOB_VOID);
        character.player.name = name;
        character.player.level = 20;
        character.player.race = RACE_HUMAN;
        character.player_index = -1;
        character.specials.position = POSITION_SLEEPING;
        character.specials2.conditions[FULL] = 24;
        character.specials2.conditions[THIRST] = 24;
        character.tmpabilities.con = 18;
        character.tmpabilities.dex = 18;
        character.tmpabilities.intel = 20;
        character.tmpabilities.wil = 10;
        character.profs->prof_level[PROF_WARRIOR] = 30;
        character.profs->prof_level[PROF_RANGER] = 15;
        character.profs->prof_level[PROF_MAGE] = 20;
        character.profs->prof_level[PROF_CLERIC] = 10;
        character.points.move_regen = kPlayerFlatMoveRegen;
        character.points.mana_regen = kPlayerFlatManaRegen;
        set_points(character, kStartingPoints, kMaximumPoints);
        place_in_room(character);
    }

    static void set_points(char_data& character, int current_points, int maximum_points)
    {
        set_pools(character, current_points, current_points, current_points);
        character.abilities.hit = maximum_points;
        character.abilities.move = maximum_points;
        character.abilities.mana = maximum_points;
    }

    static void set_pools(char_data& character, int hit_points, int moves, int mana)
    {
        character.tmpabilities.hit = hit_points;
        character.tmpabilities.move = moves;
        character.tmpabilities.mana = mana;
    }

    static void place_in_room(char_data& character)
    {
        character.in_room = kRegenRoom;
        character.next_in_room = world[kRegenRoom].people;
        world[kRegenRoom].people = &character;
    }

    // Makes `characters` the whole of character_list, in pass order.
    static void register_characters(std::initializer_list<char_data*> characters)
    {
        character_list = nullptr;
        char_data* previous = nullptr;
        for (char_data* character : characters) {
            character->next = nullptr;
            if (previous == nullptr) {
                character_list = character;
            } else {
                previous->next = character;
            }
            previous = character;
        }
    }

    // Queues the three rolls fast_update() draws for one character, in its draw order.
    static void queue_character_rolls(double hit_roll, double move_roll, double mana_roll)
    {
        push_test_random_value(hit_roll);
        push_test_random_value(move_roll);
        push_test_random_value(mana_roll);
    }

    // Queues `roll` for every draw fast_update() makes for `character_count` characters.
    static void queue_rolls(double roll, int character_count)
    {
        for (int character_index = 0; character_index < character_count; ++character_index) {
            queue_character_rolls(roll, roll, roll);
        }
    }

    // Gives `character` both markers the after-regen checks change, using `marker` as its
    // asphyxiation affect.
    static void add_check_markers(char_data& character, affected_type& marker)
    {
        marker = affected_type {};
        marker.type = SPELL_ASPHYXIATION;
        marker.duration = 100;
        marker.modifier = kAsphyxiationMarker;
        marker.location = APPLY_NONE;
        marker.next = character.affected;
        character.affected = &marker;
        character.player.ranking = kRankingMarker;
    }

    // Turns m_dying_npc into an NPC whose death special keeps it alive, losing far more hit
    // points per pass than it has. It stands in kRegenRoom but is not registered.
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

    char_data m_player {}; // the first sleeping player; first in character_list by default
    char_data m_second_player {}; // a second sleeping player; registered only by tests that use it
    char_data m_npc {}; // the level-30 NPC without a special; second in character_list by default
    char_data m_dying_npc {}; // built only by build_dying_npc(); otherwise unregistered
    char m_player_name[16] = "Sleeper"; // m_player's name storage
    char m_second_player_name[16] = "Dozer"; // m_second_player's name storage
    char m_npc_name[16] = "a test orc"; // m_npc's name storage
    char m_dying_npc_name[24] = "a withering orc"; // m_dying_npc's name storage
    index_data m_mob_index[1] {}; // mob_index for the test: entry 0 is m_dying_npc's death special
    affected_type m_markers[3] {}; // asphyxiation markers add_check_markers() links onto characters

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
    ASSERT_EQ(static_cast<int>(kPlayerHourlyHitGain / FAST_UPDATE_RATE), kPlayerWholeHitPointsPerPass);
    ASSERT_EQ(static_cast<int>(kPlayerHourlyMoveGain / FAST_UPDATE_RATE), kPlayerWholeMovesPerPass);
    ASSERT_EQ(static_cast<int>(kPlayerHourlyManaGain / FAST_UPDATE_RATE), kPlayerWholeManaPerPass);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_player), kStartingPoints + kPlayerWholeHitPointsPerPass)
        << describe_gain(kPlayerHourlyHitGain, kRollAddsNothing);
    EXPECT_EQ(GET_MOVE(&m_player), kStartingPoints + kPlayerWholeMovesPerPass)
        << describe_gain(kPlayerHourlyMoveGain, kRollAddsNothing);
    EXPECT_EQ(GET_MANA(&m_player), kStartingPoints + kPlayerWholeManaPerPass)
        << describe_gain(kPlayerHourlyManaGain, kRollAddsNothing);
}

TEST_F(FastUpdateRegenTest, ARollBelowAGainsFractionAddsOneMorePoint)
{
    queue_rolls(kRollAddsOnePoint, 2);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_player), kStartingPoints + kPlayerWholeHitPointsPerPass + 1)
        << describe_gain(kPlayerHourlyHitGain, kRollAddsOnePoint);
    EXPECT_EQ(GET_MOVE(&m_player), kStartingPoints + kPlayerWholeMovesPerPass + 1)
        << describe_gain(kPlayerHourlyMoveGain, kRollAddsOnePoint);
    EXPECT_EQ(GET_MANA(&m_player), kStartingPoints + kPlayerWholeManaPerPass + 1)
        << describe_gain(kPlayerHourlyManaGain, kRollAddsOnePoint);
}

// The three rolls are chosen so any other draw order changes at least one pool.
TEST_F(FastUpdateRegenTest, RollsAreDrawnForHitPointsThenMovesThenMana)
{
    queue_character_rolls(kRollAddsNothing, kRollAddsOnlyToPlayerMoves, kRollAddsOnePoint);
    queue_rolls(kRollAddsNothing, 1);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_player), kStartingPoints + kPlayerWholeHitPointsPerPass)
        << describe_gain(kPlayerHourlyHitGain, kRollAddsNothing);
    EXPECT_EQ(GET_MOVE(&m_player), kStartingPoints + kPlayerWholeMovesPerPass + 1)
        << describe_gain(kPlayerHourlyMoveGain, kRollAddsOnlyToPlayerMoves);
    EXPECT_EQ(GET_MANA(&m_player), kStartingPoints + kPlayerWholeManaPerPass + 1)
        << describe_gain(kPlayerHourlyManaGain, kRollAddsOnePoint);
}

// The player's hit points are already full, yet it still draws three rolls, so the NPC after it
// draws the fourth roll for its hit points.
TEST_F(FastUpdateRegenTest, EachCharacterDrawsThreeRollsEvenForAFullPool)
{
    set_pools(m_player, kMaximumPoints, kStartingPoints, kStartingPoints);
    queue_rolls(kRollAddsNothing, 1);
    queue_character_rolls(kRollAddsOnePoint, kRollAddsNothing, kRollAddsNothing);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_player), kMaximumPoints);
    EXPECT_EQ(GET_HIT(&m_npc), kStartingPoints + kNpcWholeHitPointsPerPass + 1)
        << describe_gain(kNpcHourlyHitGain, kRollAddsOnePoint);
    EXPECT_EQ(GET_MOVE(&m_npc), kStartingPoints + kNpcWholeMovesPerPass)
        << describe_gain(kNpcHourlyMoveGain, kRollAddsNothing);
    EXPECT_EQ(GET_MANA(&m_npc), kStartingPoints + kNpcWholeManaPerPass)
        << describe_gain(kNpcHourlyManaGain, kRollAddsNothing);
}

TEST_F(FastUpdateRegenTest, APassGivesAnNpcItsHourlyGainsDividedByTheFastUpdateRate)
{
    ASSERT_FLOAT_EQ(hit_gain(&m_npc), kNpcHourlyHitGain);
    ASSERT_FLOAT_EQ(move_gain(&m_npc), kNpcHourlyMoveGain);
    ASSERT_FLOAT_EQ(mana_gain(&m_npc), kNpcHourlyManaGain);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_npc), kStartingPoints + kNpcWholeHitPointsPerPass)
        << describe_gain(kNpcHourlyHitGain, kRollAddsNothing);
    EXPECT_EQ(GET_MOVE(&m_npc), kStartingPoints + kNpcWholeMovesPerPass)
        << describe_gain(kNpcHourlyMoveGain, kRollAddsNothing);
    EXPECT_EQ(GET_MANA(&m_npc), kStartingPoints + kNpcWholeManaPerPass)
        << describe_gain(kNpcHourlyManaGain, kRollAddsNothing);
}

// Each pool has its own maximum and gains more than one point, so only its own cap stops it.
TEST_F(FastUpdateRegenTest, EachPoolStopsAtItsOwnMaximum)
{
    constexpr int kMaximumHitPoints = 400;
    constexpr int kMaximumMoves = 450;
    constexpr int kMaximumMana = 500;
    m_player.abilities.hit = kMaximumHitPoints;
    m_player.abilities.move = kMaximumMoves;
    m_player.abilities.mana = kMaximumMana;
    set_pools(m_player, kMaximumHitPoints - 1, kMaximumMoves - 1, kMaximumMana - 1);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_player), kMaximumHitPoints);
    EXPECT_EQ(GET_MOVE(&m_player), kMaximumMoves);
    EXPECT_EQ(GET_MANA(&m_player), kMaximumMana);
}

TEST_F(FastUpdateRegenTest, APoolAboveItsMaximumDropsToTheMaximum)
{
    constexpr int kAboveMaximum = kMaximumPoints + 100;
    set_pools(m_player, kAboveMaximum, kAboveMaximum, kAboveMaximum);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_player), kMaximumPoints);
    EXPECT_EQ(GET_MOVE(&m_player), kMaximumPoints);
    EXPECT_EQ(GET_MANA(&m_player), kMaximumPoints);
}

TEST_F(FastUpdateRegenTest, NegativeRegenDrainsAFullPool)
{
    constexpr float kHourlyManaGain = -60.0f; // exactly -3 per pass
    m_player.points.mana_regen = static_cast<int>(kHourlyManaGain - kPlayerFormulaManaGain);
    set_pools(m_player, kStartingPoints, kStartingPoints, kMaximumPoints);
    ASSERT_FLOAT_EQ(mana_gain(&m_player), kHourlyManaGain);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(GET_MANA(&m_player), kMaximumPoints - 3)
        << describe_gain(kHourlyManaGain, kRollAddsNothing);
}

TEST_F(FastUpdateRegenTest, ARollBelowANegativeGainsFractionTakesOneMorePoint)
{
    constexpr float kHourlyManaGain = -50.0f; // -2.5 per pass
    m_player.points.mana_regen = static_cast<int>(kHourlyManaGain - kPlayerFormulaManaGain);
    ASSERT_FLOAT_EQ(mana_gain(&m_player), kHourlyManaGain);
    queue_character_rolls(kRollAddsNothing, kRollAddsNothing, kRollAddsOnePoint);
    queue_rolls(kRollAddsNothing, 1);

    fast_update();

    EXPECT_EQ(GET_MANA(&m_player), kStartingPoints - 3)
        << describe_gain(kHourlyManaGain, kRollAddsOnePoint);
}

// Fixes the current behaviour: a negative per-pass gain between -1 and 0 truncates to 0, and a
// roll below its fraction then adds a point instead of taking one.
TEST_F(FastUpdateRegenTest, LegacyARollBelowASmallNegativeGainsFractionAddsAPoint)
{
    constexpr float kHourlyManaGain = -10.0f; // -0.5 per pass
    m_player.points.mana_regen = static_cast<int>(kHourlyManaGain - kPlayerFormulaManaGain);
    ASSERT_FLOAT_EQ(mana_gain(&m_player), kHourlyManaGain);
    queue_character_rolls(kRollAddsNothing, kRollAddsNothing, kRollAddsOnePoint);
    queue_rolls(kRollAddsNothing, 1);

    fast_update();

    EXPECT_EQ(GET_MANA(&m_player), kStartingPoints + 1)
        << describe_gain(kHourlyManaGain, kRollAddsOnePoint);
}

TEST_F(FastUpdateRegenTest, MovesNeverDropBelowZero)
{
    constexpr int kDrainingMoveRegen = -2000;
    m_player.points.move_regen = kDrainingMoveRegen;
    set_pools(m_player, kStartingPoints, 1, kStartingPoints);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(GET_MOVE(&m_player), 0)
        << describe_gain(kPlayerFormulaMoveGain + kDrainingMoveRegen, kRollAddsNothing);
}

TEST_F(FastUpdateRegenTest, TheBreathingAndFameWarChecksRunForEveryCharacter)
{
    add_check_markers(m_player, m_markers[0]);
    add_check_markers(m_npc, m_markers[1]);
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(m_markers[0].modifier, kAsphyxiationMarker - 1) << "check_breathing() on the player";
    EXPECT_EQ(m_player.player.ranking, 0) << "do_fame_war_bonuses() on the unranked player";
    EXPECT_EQ(m_markers[1].modifier, kAsphyxiationMarker - 1) << "check_breathing() on the NPC";
}

// A pass that leaves a character on exactly 0 hit points does not kill it.
TEST_F(FastUpdateRegenTest, RegenThatLeavesZeroHitPointsDoesNotKill)
{
    constexpr float kHourlyHitGain = -100.0f; // exactly -5 per pass
    build_dying_npc();
    m_dying_npc.points.health_regen = static_cast<int>(kHourlyHitGain - kNpcHourlyHitGain);
    GET_HIT(&m_dying_npc) = 5;
    ASSERT_FLOAT_EQ(hit_gain(&m_dying_npc), kHourlyHitGain);
    register_characters({ &m_dying_npc, &m_player });
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(g_death_special_calls, 0);
    EXPECT_EQ(GET_HIT(&m_dying_npc), 0) << describe_gain(kHourlyHitGain, kRollAddsNothing);
    EXPECT_EQ(GET_HIT(&m_player), kStartingPoints + kPlayerWholeHitPointsPerPass)
        << "the pass should go on to the next character";
}

// Only negative regen kills: a character below zero that is healing survives the pass.
TEST_F(FastUpdateRegenTest, ACharacterBelowZeroThatIsHealingDoesNotDie)
{
    build_dying_npc();
    m_dying_npc.points.health_regen = 0;
    GET_HIT(&m_dying_npc) = -10;
    register_characters({ &m_dying_npc, &m_player });
    queue_rolls(kRollAddsNothing, 2);

    fast_update();

    EXPECT_EQ(g_death_special_calls, 0);
    EXPECT_EQ(GET_HIT(&m_dying_npc), -10 + kNpcWholeHitPointsPerPass)
        << describe_gain(kNpcHourlyHitGain, kRollAddsNothing);
    EXPECT_EQ(GET_HIT(&m_player), kStartingPoints + kPlayerWholeHitPointsPerPass)
        << "the pass should go on to the next character";
}

// Fixes the current behaviour: the pass stops at a regen death, before the dead character's
// mana, moves and after-regen checks, so characters after it in character_list miss that pass.
TEST_F(FastUpdateRegenTest, LegacyARegenDeathEndsThePassBeforeTheNextCharacter)
{
    build_dying_npc();
    add_check_markers(m_player, m_markers[0]);
    add_check_markers(m_dying_npc, m_markers[1]);
    add_check_markers(m_second_player, m_markers[2]);
    register_characters({ &m_player, &m_dying_npc, &m_second_player });
    queue_rolls(kRollAddsNothing, 3);

    fast_update();

    EXPECT_EQ(GET_HIT(&m_player), kStartingPoints + kPlayerWholeHitPointsPerPass)
        << "the character before the death regenerates";
    EXPECT_EQ(m_markers[0].modifier, kAsphyxiationMarker - 1) << "check_breathing() on the first player";
    EXPECT_EQ(m_player.player.ranking, 0) << "do_fame_war_bonuses() on the first player";

    EXPECT_EQ(g_death_special_calls, 1) << "the NPC should have died of its regen";
    EXPECT_LT(GET_HIT(&m_dying_npc), 0);
    EXPECT_EQ(GET_MOVE(&m_dying_npc), kStartingPoints)
        << "the death comes before moves would gain " << kNpcWholeMovesPerPass << " ("
        << describe_gain(kNpcHourlyMoveGain, kRollAddsNothing) << ")";
    EXPECT_EQ(GET_MANA(&m_dying_npc), kStartingPoints)
        << "the death comes before mana would gain " << kNpcWholeManaPerPass << " ("
        << describe_gain(kNpcHourlyManaGain, kRollAddsNothing) << ")";
    EXPECT_EQ(m_markers[1].modifier, kAsphyxiationMarker) << "the death comes before check_breathing()";

    EXPECT_EQ(GET_HIT(&m_second_player), kStartingPoints)
        << "the pass should end at the death, before the hit point gain of "
        << kPlayerWholeHitPointsPerPass;
    EXPECT_EQ(GET_MOVE(&m_second_player), kStartingPoints);
    EXPECT_EQ(GET_MANA(&m_second_player), kStartingPoints);
    EXPECT_EQ(m_markers[2].modifier, kAsphyxiationMarker) << "no check_breathing() after the death";
    EXPECT_EQ(m_second_player.player.ranking, kRankingMarker) << "no do_fame_war_bonuses() after the death";
}

} // namespace
