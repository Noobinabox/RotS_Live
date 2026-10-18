// Pins one regeneration_update() pass: each character gains a FAST_UPDATE_RATE-th of its hourly
// hit points, moves and mana, the fraction is settled by one random roll per point pool, every
// pool is capped at its maximum, moves never drop below zero, and a death from negative regen
// ends the pass. Pins one fast_update() pass: it changes no pool and draws no rolls, and runs the
// breathing and Fame War checks for every character, including those after a regen death.

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
// limits.cpp: not declared in limits.h.
void regeneration_update();
void fast_update();

namespace {

// A room no other suite in the binary uses, below the 1024 that gtest_main.cpp allocates.
constexpr int REGEN_ROOM = 1012;

// Whether REGEN_ROOM lies in the world's base allocation, before its extension rooms; world[]
// silently aliases an index outside it onto another room.
bool regen_room_is_allocated()
{
    return REGEN_ROOM < room_data::BASE_LENGTH - EXTENSION_SIZE;
}

// A roll at or above any fraction a gain leaves, so a pass adds only the whole points.
constexpr double ROLL_ADDS_NOTHING = 0.999999;
// A roll below any non-zero fraction, so a pass adds one point more than the whole points.
constexpr double ROLL_ADDS_ONE_POINT = 0.0;
// Above the player's hit (0.25) and mana (about 0.1) fractions and below its move fraction
// (0.40625), so it adds a point only when it is drawn for moves.
constexpr double ROLL_ADDS_ONLY_TO_PLAYER_MOVES = 0.3;

// The sleeping player's hourly gains from limits.cpp's formulas and the stats set in
// build_player():
// hit points (8 + 18 con / 2 + 30 warrior / 3 + 15 ranger / 5) * 1.5 sleeping = 45,
// moves (7 + (18 con + 18 dex) / 2 + 15 ranger / 6) * 1.75 sleeping = 48.125,
// mana (8 + 20 int / 2 + 10 wil / 5 + 20 mage / 5 + 10 cleric / 5) * 2 sleeping = 52.
// The flat move and mana regen are added after the formula.
constexpr float PLAYER_HOURLY_HIT_GAIN = 45.0f;
constexpr float PLAYER_FORMULA_MOVE_GAIN = 48.125f;
constexpr float PLAYER_FORMULA_MANA_GAIN = 52.0f;
constexpr int PLAYER_FLAT_MOVE_REGEN = 40;
constexpr int PLAYER_FLAT_MANA_REGEN = 10;
constexpr float PLAYER_HOURLY_MOVE_GAIN
    = PLAYER_FORMULA_MOVE_GAIN + PLAYER_FLAT_MOVE_REGEN; // 88.125
constexpr float PLAYER_HOURLY_MANA_GAIN
    = PLAYER_FORMULA_MANA_GAIN + PLAYER_FLAT_MANA_REGEN; // 62
// The whole points per pass, different for each pool so a pool given another's value is caught.
// They, and the per-pass figures below, are worked out for this rate.
static_assert(FAST_UPDATE_RATE == 20, "the per-pass constants assume 20 fast updates an hour");
constexpr int PLAYER_WHOLE_HIT_POINTS_PER_PASS = 2; // 2.25
constexpr int PLAYER_WHOLE_MOVES_PER_PASS = 4; // 4.40625
constexpr int PLAYER_WHOLE_MANA_PER_PASS = 3; // about 3.1

// The level-30 NPC's hourly gains: an NPC out of combat gains its level * 1.5 hit points and
// mana, and 7 + (20 con + 20 dex) / 2 + 30 level / 6 + 80 default knowledge / 40 - 5 = 29 moves.
constexpr float NPC_HOURLY_HIT_GAIN = 45.0f;
constexpr float NPC_HOURLY_MOVE_GAIN = 29.0f;
constexpr float NPC_HOURLY_MANA_GAIN = 45.0f;
constexpr int NPC_WHOLE_HIT_POINTS_PER_PASS = 2; // 2.25
constexpr int NPC_WHOLE_MOVES_PER_PASS = 1; // 1.45
constexpr int NPC_WHOLE_MANA_PER_PASS = 2; // 2.25

// Every pool starts here, below its maximum, unless a test sets it otherwise.
constexpr int STARTING_POINTS = 100;
constexpr int MAXIMUM_POINTS = 500;

// A flat health regen far below any natural gain, so one pass takes a character from
// STARTING_POINTS to below zero.
constexpr int LETHAL_HOURLY_HEALTH_REGEN = -1000 * FAST_UPDATE_RATE;

// check_breathing() lowers an asphyxiation affect's modifier by one each pass a character can
// breathe; above 1, so a pass leaves the affect in place.
constexpr int ASPHYXIATION_MARKER = 5;
// do_fame_war_bonuses() sets an unranked player's ranking to 0; any other value shows whether
// it ran.
constexpr int RANKING_MARKER = 7;

// How many times dying_npc_special() has been asked about a death since the fixture's SetUp().
int g_death_special_calls = 0;

// A mob special that claims every death it is asked about, so raw_kill() returns before it
// extracts and frees the NPC and the test can still read the NPC after the pass.
int dying_npc_special(char_data* /*host*/, char_data* /*character*/, int /*cmd*/,
    char* /*argument*/, int callflag, waiting_type* /*wait_data*/)
{
    if (callflag == SPECIAL_DEATH) {
        ++g_death_special_calls;
        return TRUE;
    }
    return FALSE;
}

// Distinct rolls a test queues to see whether a pass drew any of them.
constexpr double UNTOUCHED_ROLLS[] = { 0.125, 0.25, 0.375 };

// Describes how a pass's change was derived, for failure messages.
std::string describe_gain(float hourly_gain, double roll)
{
    std::ostringstream description;
    description << "hourly gain " << hourly_gain << " / FAST_UPDATE_RATE " << FAST_UPDATE_RATE
                << ", roll " << roll;
    return description.str();
}

// Two sleeping human players and one level-30 NPC standing in REGEN_ROOM with known stats, the
// first player and the NPC registered in character_list, and the global lists the passes and
// raw_kill() walk isolated from the rest of the binary.
class FastUpdateRegenTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_TRUE(regen_room_is_allocated())
            << "Expected room " << REGEN_ROOM << " inside the world allocation of "
            << room_data::BASE_LENGTH - EXTENSION_SIZE << " rooms from gtest_main.cpp.";
        m_saved_character_list = character_list;
        m_saved_waiting_list = waiting_list;
        m_saved_mob_index = mob_index;
        m_saved_top_of_mobt = top_of_mobt;
        m_saved_room_people = world[REGEN_ROOM].people;
        m_saved_room_sector = world[REGEN_ROOM].sector_type;

        // raw_kill() walks waiting_list; earlier suites can leave stale entries on it.
        waiting_list = nullptr;
        m_mob_index[0].func = dying_npc_special;
        mob_index = m_mob_index;
        top_of_mobt = 0;
        world[REGEN_ROOM].sector_type = SECT_INSIDE;
        world[REGEN_ROOM].people = nullptr;
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
        set_points(m_npc, STARTING_POINTS, MAXIMUM_POINTS);
        place_in_room(m_npc);

        register_characters({ &m_player, &m_npc });
    }

    void TearDown() override
    {
        // SetUp() saved nothing when it stopped on an unallocated room.
        if (!regen_room_is_allocated()) {
            return;
        }
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
        world[REGEN_ROOM].people = m_saved_room_people;
        world[REGEN_ROOM].sector_type = m_saved_room_sector;
    }

    // Makes `out_character` the sleeping level-20 human player whose gains the PLAYER_
    // constants describe, standing in REGEN_ROOM but not registered in character_list.
    void build_player(char_data& out_character, char* name)
    {
        clear_char(&out_character, MOB_VOID);
        out_character.player.name = name;
        out_character.player.level = 20;
        out_character.player.race = RACE_HUMAN;
        out_character.player_index = -1;
        out_character.specials.position = POSITION_SLEEPING;
        out_character.specials2.conditions[FULL] = 24;
        out_character.specials2.conditions[THIRST] = 24;
        out_character.tmpabilities.con = 18;
        out_character.tmpabilities.dex = 18;
        out_character.tmpabilities.intel = 20;
        out_character.tmpabilities.wil = 10;
        out_character.profs->prof_level[PROF_WARRIOR] = 30;
        out_character.profs->prof_level[PROF_RANGER] = 15;
        out_character.profs->prof_level[PROF_MAGE] = 20;
        out_character.profs->prof_level[PROF_CLERIC] = 10;
        out_character.points.move_regen = PLAYER_FLAT_MOVE_REGEN;
        out_character.points.mana_regen = PLAYER_FLAT_MANA_REGEN;
        set_points(out_character, STARTING_POINTS, MAXIMUM_POINTS);
        place_in_room(out_character);
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
        character.in_room = REGEN_ROOM;
        character.next_in_room = world[REGEN_ROOM].people;
        world[REGEN_ROOM].people = &character;
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

    // Queues the three rolls regeneration_update() draws for one character, in its draw order.
    static void queue_character_rolls(double hit_roll, double move_roll, double mana_roll)
    {
        push_test_random_value(hit_roll);
        push_test_random_value(move_roll);
        push_test_random_value(mana_roll);
    }

    // Queues `roll` for every draw regeneration_update() makes for `character_count` characters.
    static void queue_rolls(double roll, int character_count)
    {
        for (int character_index = 0; character_index < character_count; ++character_index) {
            queue_character_rolls(roll, roll, roll);
        }
    }

    // Gives `character` both markers fast_update()'s checks change, using `out_marker` as its
    // asphyxiation affect.
    static void add_check_markers(char_data& character, affected_type& out_marker)
    {
        out_marker = affected_type {};
        out_marker.type = SPELL_ASPHYXIATION;
        out_marker.duration = 100;
        out_marker.modifier = ASPHYXIATION_MARKER;
        out_marker.location = APPLY_NONE;
        out_marker.next = character.affected;
        character.affected = &out_marker;
        character.player.ranking = RANKING_MARKER;
    }

    // Turns m_dying_npc into an NPC whose death special keeps it alive, losing far more hit
    // points per pass than it has. It stands in REGEN_ROOM but is not registered.
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
        m_dying_npc.points.health_regen = LETHAL_HOURLY_HEALTH_REGEN;
        set_points(m_dying_npc, STARTING_POINTS, MAXIMUM_POINTS);
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
    char_data* m_saved_room_people = nullptr; // REGEN_ROOM's occupants, restored in TearDown()
    int m_saved_room_sector = 0; // REGEN_ROOM's sector, restored in TearDown()
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
}

// The pass stops at a regen death, before the dead character's moves and mana, so characters
// after it in character_list do not regenerate on that pass.
TEST_F(FastUpdateRegenTest, ARegenDeathStopsRegenerationBeforeTheNextCharacter)
{
    build_dying_npc();
    register_characters({ &m_player, &m_dying_npc, &m_second_player });
    queue_rolls(ROLL_ADDS_NOTHING, 3);

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
}

// Runs the two passes in the heartbeat's order: a regen death stops regeneration, but the
// characters after it in character_list still get the checks.
TEST_F(FastUpdateRegenTest, TheChecksStillReachCharactersAfterARegenDeath)
{
    build_dying_npc();
    add_check_markers(m_player, m_markers[0]);
    add_check_markers(m_second_player, m_markers[2]);
    register_characters({ &m_player, &m_dying_npc, &m_second_player });
    queue_rolls(ROLL_ADDS_NOTHING, 3);

    regeneration_update();
    fast_update();

    ASSERT_EQ(g_death_special_calls, 1) << "the NPC should have died of its regen";
    EXPECT_EQ(GET_HIT(&m_second_player), STARTING_POINTS)
        << "the character after the death should not regenerate";
    EXPECT_EQ(m_markers[0].modifier, ASPHYXIATION_MARKER - 1)
        << "check_breathing() on the first player";
    EXPECT_EQ(m_player.player.ranking, 0) << "do_fame_war_bonuses() on the first player";
    EXPECT_EQ(m_markers[2].modifier, ASPHYXIATION_MARKER - 1)
        << "check_breathing() on the player after the death";
    EXPECT_EQ(m_second_player.player.ranking, 0)
        << "do_fame_war_bonuses() on the player after the death";
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

// The checks reach every character, including those after one whose regen would be lethal.
TEST_F(FastUpdateRegenTest, FastUpdateRunsTheChecksPastACharacterWithLethalRegen)
{
    build_dying_npc();
    add_check_markers(m_player, m_markers[0]);
    add_check_markers(m_dying_npc, m_markers[1]);
    add_check_markers(m_second_player, m_markers[2]);
    register_characters({ &m_player, &m_dying_npc, &m_second_player });

    fast_update();

    EXPECT_EQ(m_markers[0].modifier, ASPHYXIATION_MARKER - 1)
        << "check_breathing() on the first player";
    EXPECT_EQ(m_player.player.ranking, 0) << "do_fame_war_bonuses() on the first player";
    EXPECT_EQ(m_markers[1].modifier, ASPHYXIATION_MARKER - 1) << "check_breathing() on the NPC";
    EXPECT_EQ(m_markers[2].modifier, ASPHYXIATION_MARKER - 1)
        << "check_breathing() on the second player";
    EXPECT_EQ(m_second_player.player.ranking, 0) << "do_fame_war_bonuses() on the second player";
}

} // namespace
