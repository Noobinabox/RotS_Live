#include "../mob_progs/ranger_prog.h"
#include "../mob_progs/shopkeeper.h"

#include "../comm.h"
#include "../db.h"
#include "../handler.h"
#include "../interpre.h"
#include "../spells.h"
#include "../structs.h"
#include "../utils.h"
#include "../zone.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern struct char_data* mob_proto;
extern struct index_data* mob_index;
extern int top_of_mobt;
extern struct room_data world;
extern int top_of_world;
extern struct descriptor_data* descriptor_list;
extern struct char_data* combat_list;
extern struct zone_data* zone_table;
extern struct time_info_data time_info;
void remove_char_exists(int num);
void set_char_exists(int num);
int char_exists(int num);
void clear_char(struct char_data* ch, int mode);
void clear_object(struct obj_data* obj);
int in_waiting_list(struct char_data* ch);
void abort_delay(struct char_data* wait_ch);
SPECIAL(mob_ranger_new);
int ranger_track_direction(char_data* host);
bool ranger_can_step(char_data* host, int dir);
void one_mobile_activity(char_data* ch);

namespace {

std::vector<std::string> problem_texts(const std::vector<vendor_problem>& problems)
{
    std::vector<std::string> out;
    for (const vendor_problem& p : problems)
        out.push_back(std::to_string(p.line) + ": " + p.text);
    return out;
}

/* ---- The settings ---- */

TEST(RangerOptions, NoTextIsAllOff)
{
    std::vector<vendor_problem> problems;
    ranger_config config = parse_ranger_options(nullptr, &problems);
    EXPECT_FALSE(config.hide);
    EXPECT_FALSE(config.ambush);
    EXPECT_FALSE(config.track);
    EXPECT_FALSE(config.riposte);
    EXPECT_TRUE(problems.empty());
}

TEST(RangerOptions, ReadsEachSetting)
{
    std::vector<vendor_problem> problems;
    ranger_config config = parse_ranger_options("hide=yes\nambush = yes\ntrack=yes\nriposte=yes", &problems);
    EXPECT_TRUE(config.hide);
    EXPECT_TRUE(config.ambush);
    EXPECT_TRUE(config.track);
    EXPECT_TRUE(config.riposte);
    EXPECT_TRUE(problems.empty());

    config = parse_ranger_options("hide=no\ntrack=yes", &problems);
    EXPECT_FALSE(config.hide);
    EXPECT_TRUE(config.track);
}

TEST(RangerOptions, BadDuplicateAndUnknownLinesAreReported)
{
    std::vector<vendor_problem> problems;
    ranger_config config = parse_ranger_options("hide=maybe\nambush=yes\nambush=no\nstab=yes\nhunt\n// a comment", &problems);
    EXPECT_FALSE(config.hide);
    EXPECT_TRUE(config.ambush) << "the first ambush line stands";
    EXPECT_EQ(problem_texts(problems),
        (std::vector<std::string> { "1: bad hide - line ignored", "3: duplicate ambush - line ignored",
            "4: unknown setting - line ignored", "5: unknown setting - line ignored" }));
}

TEST(RangerOptions, WalkerLinesArePassedOver)
{
    std::vector<vendor_problem> problems;
    ranger_config config = parse_ranger_options("walker_type=path\nwalker_rooms=1-3\ntrack=yes", &problems);
    EXPECT_TRUE(config.track);
    EXPECT_TRUE(problems.empty());
}

TEST(RangerOptions, UnsupportedFlagsAreWarnedWithinSeventyEightColumns)
{
    char_data proto;
    clear_char(&proto, MOB_ISNPC);
    proto.specials2.act = MOB_ISNPC | MOB_SPEC;
    EXPECT_TRUE(ranger_flag_warnings(&proto, 19602).empty());

    proto.specials2.act |= MOB_SCAVENGER | MOB_BODYGUARD | MOB_ASSISTANT | MOB_HELPER;
    std::vector<std::string> lines = ranger_flag_warnings(&proto, 1999999);
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "MOB WARNING: mobile #1999999: scavenger has no effect with program 32");
    EXPECT_EQ(lines[1], "MOB WARNING: mobile #1999999: bodyguard has no effect with program 32");
    EXPECT_EQ(lines[2], "MOB WARNING: mobile #1999999: assistant has no effect with program 32");
    for (const std::string& line : lines)
        EXPECT_LE(line.size(), 78u) << line;
}

/* ---- The shared aggression test ---- */

TEST(MobAggression, FlagFollowsTheAlignmentLimits)
{
    char_data mob, good, evil, npc;
    clear_char(&mob, MOB_ISNPC);
    clear_char(&good, 0);
    clear_char(&evil, 0);
    clear_char(&npc, MOB_ISNPC);
    npc.specials2.act = MOB_ISNPC;
    good.specials2.alignment = 500;
    evil.specials2.alignment = -500;

    EXPECT_FALSE(mob_flag_aggressive_to(&mob, &good)) << "not AGGRESSIVE";
    mob.specials2.act = MOB_ISNPC | MOB_AGGRESSIVE;
    EXPECT_TRUE(mob_flag_aggressive_to(&mob, &good));
    EXPECT_TRUE(mob_flag_aggressive_to(&mob, &evil));
    EXPECT_FALSE(mob_flag_aggressive_to(&mob, &npc)) << "the flag never targets mobs";

    mob.specials2.act |= MOB_AGGRESSIVE_EVIL;
    EXPECT_FALSE(mob_flag_aggressive_to(&mob, &good));
    EXPECT_TRUE(mob_flag_aggressive_to(&mob, &evil));

    mob.specials2.act |= MOB_AGGRESSIVE_GOOD;
    EXPECT_TRUE(mob_flag_aggressive_to(&mob, &good));

    mob.specials2.act |= MOB_MOUNT;
    EXPECT_FALSE(mob_flag_aggressive_to(&mob, &evil)) << "a mount is never aggressive";
}

TEST(MobAggression, RaceNeedsPreferencesAndNoMount)
{
    char_data mob, orc;
    clear_char(&mob, MOB_ISNPC);
    clear_char(&orc, 0);
    mob.specials2.act = MOB_ISNPC;
    orc.player.race = RACE_ORC;

    EXPECT_FALSE(mob_race_aggressive_to(&mob, &orc));
    mob.specials2.pref = 1 << RACE_ORC;
    EXPECT_TRUE(mob_race_aggressive_to(&mob, &orc));
    mob.specials2.act |= MOB_MOUNT;
    EXPECT_FALSE(mob_race_aggressive_to(&mob, &orc));
}

/* ---- The program's turn ----
 *
 * Two rooms, A (index 990) and B (991), joined north-south. The ranger mob
 * and a level 20 human player start in A. */

class RangerProgTest : public ::testing::Test {
protected:
    static constexpr int kRoomC = 989; /* only set up by tests that need a third room */
    static constexpr int kRoomA = 990;
    static constexpr int kRoomB = 991;
    static constexpr int kRoomCount = 3; /* 989..991 */
    static constexpr int kMobSlot = 4600;
    static constexpr int kPlayerSlot = 4601;
    static constexpr int kAllySlot = 4602;
    static constexpr int kEnemySlot = 4603;

    void SetUp() override
    {
        /* The fight list is global: what an earlier suite left on it is put
         * aside, so these tests only ever see their own fighters. */
        m_saved_combat_list = combat_list;
        combat_list = nullptr;
        m_saved_mob_proto = mob_proto;
        m_saved_mob_index = mob_index;
        m_saved_top_of_mobt = top_of_mobt;
        m_saved_descriptor_list = descriptor_list;
        m_saved_zone_table = zone_table;
        m_saved_top_of_world = top_of_world;
        top_of_world = kRoomB; /* the path finder searches rooms 0..top_of_world */
        descriptor_list = nullptr;
        zone_table = m_zone_table;
        for (int i = 0; i < kRoomCount; ++i)
            m_saved_rooms[i] = world[kRoomC + i];
        m_next_seed = std::rand(); /* later tests go on from here, not from seed 1 */
        std::srand(1);

        for (int room : { kRoomA, kRoomB }) {
            world[room].number = 99000 + room;
            world[room].zone = 0;
            world[room].light = 1;
            world[room].room_flags = 0;
            world[room].sector_type = SECT_FIELD;
            world[room].people = nullptr;
            world[room].contents = nullptr;
            for (int dir = 0; dir < NUM_OF_DIRS; ++dir)
                world[room].dir_option[dir] = nullptr;
            for (room_track_data& track : world[room].room_track)
                track = room_track_data();
        }
        link(kRoomA, NORTH, kRoomB);
        link(kRoomB, SOUTH, kRoomA);

        clear_char(&m_mob_proto[0], MOB_ISNPC);
        m_mob_proto[0].specials2.act = MOB_ISNPC | MOB_SPEC;
        m_mob_proto[0].specials.store_prog_number = PROG_RANGER;
        m_mob_proto[0].specials.mob_options = m_options;
        m_mob_index[0].virt = 19602;
        m_mob_index[0].func = nullptr;
        mob_proto = m_mob_proto;
        mob_index = m_mob_index;
        top_of_mobt = 0;

        make_mob(&m_mob, kMobSlot, m_mob_name);
        make_player(&m_player, kPlayerSlot);
        m_ally.in_room = m_enemy.in_room = NOWHERE; /* placed only by the tests that use them */
        place(&m_mob, kRoomA);
        place(&m_player, kRoomA);
        set_options("");
    }

    void TearDown() override
    {
        for (char_data* ch : { &m_mob, &m_player, &m_ally, &m_enemy }) {
            /* An attack puts the attacker on the waiting list (do_hit's wait state). */
            if (in_waiting_list(ch) || ch->delay.wait_value)
                abort_delay(ch);
            if (ch->specials.fighting)
                stop_fighting(ch);
            while (ch->specials.memory)
                forget(ch, ch->specials.memory->enemy);
            if (ch->in_room != NOWHERE)
                char_from_room(ch);
        }
        /* stop_fighting leaves a fighter short of energy on the list, so the
         * list is emptied of this fixture's characters here. */
        combat_list = m_saved_combat_list;
        remove_char_exists(kMobSlot);
        remove_char_exists(kPlayerSlot);
        remove_char_exists(kAllySlot);
        remove_char_exists(kEnemySlot);
        m_mob_proto[0].specials.store_prog_number = 0;
        ranger_config_rebuild(0, nullptr, false); /* erases the registry entry */
        for (int room : { kRoomC, kRoomA, kRoomB }) {
            if (room == kRoomC && !m_room_c_made)
                continue; /* its exits are not ours */
            for (int dir = 0; dir < NUM_OF_DIRS; ++dir) {
                delete world[room].dir_option[dir];
                world[room].dir_option[dir] = nullptr;
            }
        }
        for (int i = 0; i < kRoomCount; ++i)
            world[kRoomC + i] = m_saved_rooms[i];
        std::srand(m_next_seed);
        mob_proto = m_saved_mob_proto;
        mob_index = m_saved_mob_index;
        top_of_mobt = m_saved_top_of_mobt;
        descriptor_list = m_saved_descriptor_list;
        zone_table = m_saved_zone_table;
        top_of_world = m_saved_top_of_world;
    }

    /* A third room, in room A's zone, with no exits. */
    void make_room_c()
    {
        m_room_c_made = true;
        world[kRoomC] = world[kRoomA];
        world[kRoomC].number = 99000 + kRoomC;
        world[kRoomC].people = nullptr;
        world[kRoomC].contents = nullptr;
        for (int dir = 0; dir < NUM_OF_DIRS; ++dir)
            world[kRoomC].dir_option[dir] = nullptr;
        for (room_track_data& track : world[kRoomC].room_track)
            track = room_track_data();
    }

    static void link(int from, int dir, int to)
    {
        room_direction_data* exit = new room_direction_data();
        exit->general_description = nullptr;
        exit->keyword = nullptr;
        exit->exit_width = 4;
        exit->exit_info = 0;
        exit->key = -1;
        exit->to_room = to;
        world[from].dir_option[dir] = exit;
    }

    static void give_stats(char_data* ch)
    {
        ch->tmpabilities.str = ch->abilities.str = 15;
        ch->tmpabilities.dex = ch->abilities.dex = 15;
        ch->tmpabilities.con = ch->abilities.con = 15;
        ch->tmpabilities.wil = ch->abilities.wil = 15;
        ch->tmpabilities.lea = ch->abilities.lea = 15;
        ch->constabilities = ch->abilities;
    }

    void make_mob(char_data* mob, int slot, char* name)
    {
        clear_char(mob, MOB_ISNPC);
        mob->nr = 0;
        mob->abs_number = slot;
        set_char_exists(slot);
        mob->specials2.act = MOB_ISNPC | MOB_SPEC;
        mob->specials.store_prog_number = PROG_RANGER;
        mob->player.name = name;
        mob->player.short_descr = name;
        mob->player.level = 20;
        mob->player.race = 0;
        mob->tmpabilities.hit = mob->abilities.hit = 200;
        mob->tmpabilities.move = mob->abilities.move = 200;
        mob->tmpabilities.intel = mob->abilities.intel = 12;
        mob->specials.position = POSITION_STANDING;
        mob->specials.default_pos = POSITION_STANDING;
        give_stats(mob);
        mob->in_room = NOWHERE;
    }

    void make_player(char_data* pc, int slot)
    {
        clear_char(pc, 0);
        pc->abs_number = slot;
        set_char_exists(slot);
        pc->player.name = m_player_name;
        pc->player.race = RACE_HUMAN;
        pc->player.level = 20;
        pc->tmpabilities.hit = pc->abilities.hit = 200;
        pc->tmpabilities.move = pc->abilities.move = 200;
        pc->specials.position = POSITION_STANDING;
        pc->specials2.alignment = 500;
        give_stats(pc);
        /* A linkless player flees from any fight, so the player is connected. */
        m_descriptor.output = m_descriptor.small_outbuf;
        m_descriptor.small_outbuf[0] = '\0';
        m_descriptor.bufspace = SMALL_BUFSIZE - 1;
        m_descriptor.connected = CON_PLYNG;
        m_descriptor.descriptor = 1;
        m_descriptor.character = pc;
        pc->desc = &m_descriptor;
        pc->in_room = NOWHERE;
    }

    static void place(char_data* ch, int room)
    {
        if (ch->in_room != NOWHERE)
            char_from_room(ch);
        char_to_room(ch, room);
    }

    void set_options(const char* text)
    {
        std::strncpy(m_options, text, sizeof(m_options) - 1);
        m_options[sizeof(m_options) - 1] = '\0';
        ranger_config_rebuild(0, nullptr, false);
    }

    /* An ally of the same alignment as the mob, fighting a mob of its own. */
    void ally_in_a_fight()
    {
        m_mob.specials2.alignment = 300;
        make_mob(&m_ally, kAllySlot, m_ally_name);
        m_ally.specials.store_prog_number = 0;
        m_ally.specials2.act = MOB_ISNPC;
        m_ally.specials2.alignment = 300;
        make_mob(&m_enemy, kEnemySlot, m_enemy_name);
        m_enemy.specials.store_prog_number = 0;
        m_enemy.specials2.act = MOB_ISNPC;
        place(&m_ally, kRoomA);
        place(&m_enemy, kRoomA);
        set_fighting(&m_ally, &m_enemy);
        set_fighting(&m_enemy, &m_ally);
    }

    void wield_dagger(obj_data* dagger)
    {
        clear_object(dagger);
        dagger->obj_flags.type_flag = ITEM_WEAPON;
        dagger->obj_flags.value[2] = 1; /* bulk */
        dagger->obj_flags.value[3] = 11; /* a stabbing weapon */
        m_mob.equipment[WIELD] = dagger;
    }

    int turn(int callflag = SPECIAL_SELF)
    {
        return mob_ranger_new(&m_mob, &m_mob, 0, m_arg, callflag, nullptr);
    }

    /* Turns until the mob leaves room A, at most `turns`; true if it did. */
    bool wanders_within(int turns)
    {
        for (int i = 0; i < turns; ++i) {
            m_mob.delay.wait_value = 0;
            turn();
            if (m_mob.in_room != kRoomA)
                return true;
        }
        return false;
    }

    /* `rider` rides the mob, as do_ride leaves them: no master while
     * ridden. A player rider is made linkless, since a step would make them
     * look, which needs room text. */
    void ride(char_data* rider, int rider_slot)
    {
        place(rider, m_mob.in_room);
        m_mob.mount_data.rider = rider;
        m_mob.mount_data.rider_number = rider_slot;
        rider->mount_data.mount = &m_mob;
        rider->mount_data.mount_number = kMobSlot;
        rider->desc = nullptr;
    }
    void ride() { ride(&m_player, kPlayerSlot); }

    void unride()
    {
        if (m_mob.mount_data.rider)
            m_mob.mount_data.rider->mount_data.mount = nullptr;
        m_mob.mount_data.rider = nullptr;
        m_mob.mount_data.rider_number = -1;
        m_player.desc = &m_descriptor;
    }

    /* The standard mob turn, on a mob with no program: true if the mob left
     * room A within `turns`. */
    bool standard_turn_wanders_within(int turns)
    {
        m_mob.specials2.act = MOB_ISNPC | MOB_MOUNT;
        m_mob.specials.store_prog_number = 0;
        for (int i = 0; i < turns; ++i) {
            m_mob.delay.wait_value = 0;
            one_mobile_activity(&m_mob);
            if (m_mob.in_room != kRoomA)
                return true;
        }
        return false;
    }

    char m_options[256] = "";
    char m_mob_name[32] = "ranger spider";
    char m_ally_name[32] = "ally wolf";
    char m_enemy_name[32] = "enemy bear";
    char m_player_name[16] = "Walker";
    char m_arg[8] = "";
    char_data m_mob_proto[1] {};
    index_data m_mob_index[1] {};
    char_data m_mob {};
    char_data m_player {};
    char_data m_ally {};
    char_data m_enemy {};
    descriptor_data m_descriptor {};

    char_data* m_saved_mob_proto = nullptr;
    index_data* m_saved_mob_index = nullptr;
    int m_saved_top_of_mobt = 0;
    descriptor_data* m_saved_descriptor_list = nullptr;
    zone_data* m_saved_zone_table = nullptr;
    int m_saved_top_of_world = 0;
    char_data* m_saved_combat_list = nullptr;
    room_data m_saved_rooms[kRoomCount] {};
    unsigned m_next_seed = 1;
    bool m_room_c_made = false;
    zone_data m_zone_table[2] {};
};

TEST_F(RangerProgTest, TheEntryCallIsIgnored)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    EXPECT_EQ(mob_ranger_new(&m_mob, &m_player, 1, m_arg, SPECIAL_ENTER, nullptr), 0);
    EXPECT_EQ(m_mob.specials.fighting, nullptr);
}

TEST_F(RangerProgTest, OwnTurnAlwaysEndsTheTurn)
{
    m_mob.specials2.act |= MOB_SENTINEL;
    EXPECT_EQ(turn(), 1);
}

TEST_F(RangerProgTest, AttacksAPlayerWithTheAggressiveFlag)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_player);
}

TEST_F(RangerProgTest, AttacksAPlayerOfAHatedRace)
{
    m_mob.specials2.act |= MOB_SENTINEL;
    m_mob.specials2.pref = 1 << RACE_HUMAN;
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_player);
}

TEST_F(RangerProgTest, AggressiveAlignmentLimitIsObeyed)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_AGGRESSIVE_EVIL | MOB_SENTINEL;
    turn();
    EXPECT_EQ(m_mob.specials.fighting, nullptr) << "the player is good";
    m_player.specials2.alignment = -500;
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_player);
}

TEST_F(RangerProgTest, NohassleImmortalIsLeftAlone)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    SET_BIT(PRF_FLAGS(&m_player), PRF_NOHASSLE);
    turn();
    EXPECT_EQ(m_mob.specials.fighting, nullptr);
}

TEST_F(RangerProgTest, NoAttackAndNoAttackLineInAPeaceRoom)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    world[kRoomA].room_flags = PEACEROOM;
    turn();
    turn();
    EXPECT_EQ(m_mob.specials.fighting, nullptr);
    EXPECT_EQ(std::string(m_descriptor.output).find("attacks"), std::string::npos) << m_descriptor.output;
}

TEST_F(RangerProgTest, AttackLineIsShownForARealAttack)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    turn();
    EXPECT_NE(std::string(m_descriptor.output).find("grins evilly and attacks"), std::string::npos)
        << m_descriptor.output;
}

TEST_F(RangerProgTest, AmbushWithoutAWeaponFallsBackToAHit)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    set_options("ambush=yes");
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_player);
}

TEST_F(RangerProgTest, AmbushWithASmallWeaponStartsTheWindUp)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    set_options("ambush=yes");
    obj_data dagger;
    wield_dagger(&dagger);
    turn();
    m_mob.equipment[WIELD] = nullptr;
    EXPECT_EQ(m_mob.delay.cmd, CMD_AMBUSH);
    EXPECT_EQ(m_mob.specials.fighting, nullptr) << "the ambush lands later, not as a hit now";
    abort_delay(&m_mob);
}

TEST_F(RangerProgTest, SwitchingTurnsToAnotherAttacker)
{
    m_mob.specials2.act |= MOB_SWITCHING | MOB_SENTINEL;
    make_mob(&m_ally, kAllySlot, m_ally_name);
    m_ally.specials.store_prog_number = 0;
    m_ally.specials2.act = MOB_ISNPC;
    place(&m_ally, kRoomA);
    set_fighting(&m_mob, &m_player);
    set_fighting(&m_player, &m_mob);
    set_fighting(&m_ally, &m_mob);
    bool switched = false;
    for (int i = 0; i < 100 && !switched; ++i) {
        turn();
        switched = m_mob.specials.fighting == &m_ally;
    }
    EXPECT_TRUE(switched);
}

TEST_F(RangerProgTest, WimpyMobAtLowHealthStartsNothing)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_WIMPY | MOB_SENTINEL;
    m_mob.tmpabilities.hit = 40; /* a fifth of 200 */
    turn();
    EXPECT_EQ(m_mob.specials.fighting, nullptr);
}

TEST_F(RangerProgTest, MobWithoutWimpyFightsAtLowHealth)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    m_mob.tmpabilities.hit = 40;
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_player);
}

TEST_F(RangerProgTest, RememberedEnemyIsAttackedWithoutAggression)
{
    m_mob.specials2.act |= MOB_MEMORY | MOB_SENTINEL;
    remember(&m_mob, &m_player);
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_player);
}

TEST_F(RangerProgTest, RememberedMasterIsForgotten)
{
    m_mob.specials2.act |= MOB_MEMORY | MOB_SENTINEL;
    remember(&m_mob, &m_player);
    m_mob.master = &m_player;
    turn();
    m_mob.master = nullptr;
    EXPECT_EQ(m_mob.specials.fighting, nullptr);
    EXPECT_EQ(m_mob.specials.memory, nullptr);
}

TEST_F(RangerProgTest, HunterStepsTowardItsRememberedEnemy)
{
    m_mob.specials2.act |= MOB_MEMORY | MOB_HUNTER | MOB_SENTINEL;
    remember(&m_mob, &m_player);
    place(&m_player, kRoomB);
    turn();
    EXPECT_EQ(m_mob.in_room, kRoomB);
}

TEST_F(RangerProgTest, TrackOfAnyRaceIsFollowedByAnAggressiveMob)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    place(&m_player, kRoomB);
    set_options("track=yes");
    world[kRoomA].room_track[0].char_number = -RACE_URUK;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    turn();
    EXPECT_EQ(m_mob.in_room, kRoomB);
}

TEST_F(RangerProgTest, TrackOfAHatedRaceIsFollowed)
{
    m_mob.specials2.act |= MOB_SENTINEL;
    m_mob.specials2.pref = 1 << RACE_ORC;
    set_options("track=yes");
    world[kRoomA].room_track[0].char_number = -RACE_ORC;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    turn();
    EXPECT_EQ(m_mob.in_room, kRoomB);
}

TEST_F(RangerProgTest, TracksAreIgnoredWithoutTheOption)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    place(&m_player, kRoomB);
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    turn();
    EXPECT_EQ(m_mob.in_room, kRoomA);
}

TEST_F(RangerProgTest, TrackThroughALockedDoorIsNotChosen)
{
    /* Tested on the choice itself: a refused step would let the turn go on to
     * wander anyway, so where the mob ends up shows nothing. */
    m_mob.specials2.act |= MOB_AGGRESSIVE;
    place(&m_player, kRoomB);
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    world[kRoomA].dir_option[NORTH]->exit_info = EX_ISDOOR | EX_CLOSED;
    EXPECT_EQ(ranger_track_direction(&m_mob), NORTH);
    world[kRoomA].dir_option[NORTH]->exit_info = EX_ISDOOR | EX_CLOSED | EX_LOCKED;
    EXPECT_EQ(ranger_track_direction(&m_mob), -1);
}

TEST_F(RangerProgTest, TrackThroughAClosedDoorIsFollowed)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    place(&m_player, kRoomB);
    set_options("track=yes");
    world[kRoomA].dir_option[NORTH]->exit_info = EX_ISDOOR | EX_CLOSED;
    world[kRoomB].dir_option[SOUTH]->exit_info = EX_ISDOOR | EX_CLOSED;
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    turn();
    EXPECT_EQ(m_mob.in_room, kRoomB) << "it opens the door and follows";
}

TEST_F(RangerProgTest, TrackOutOfItsZoneIsNotChosenForAStayZoneMob)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE;
    place(&m_player, kRoomB);
    world[kRoomB].zone = 1;
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    EXPECT_EQ(ranger_track_direction(&m_mob), NORTH);
    m_mob.specials2.act |= MOB_STAY_ZONE;
    EXPECT_EQ(ranger_track_direction(&m_mob), -1);
}

TEST_F(RangerProgTest, TheStepCheckRefusesWhatTheMoveWouldRefuse)
{
    /* The check the hunt and track steps make before trying a step. */
    EXPECT_TRUE(ranger_can_step(&m_mob, NORTH));
    EXPECT_FALSE(ranger_can_step(&m_mob, EAST)) << "no exit";
    world[kRoomB].zone = 1;
    m_mob.specials2.act |= MOB_STAY_ZONE;
    EXPECT_FALSE(ranger_can_step(&m_mob, NORTH)) << "out of its zone";
    m_mob.specials2.act &= ~MOB_STAY_ZONE;
    world[kRoomB].room_flags = NO_MOB;
    EXPECT_FALSE(ranger_can_step(&m_mob, NORTH)) << "a NO_MOB room";
    world[kRoomB].room_flags = 0;
    world[kRoomA].dir_option[NORTH]->exit_info = EX_ISDOOR | EX_CLOSED;
    EXPECT_TRUE(ranger_can_step(&m_mob, NORTH)) << "a door it opens";
    world[kRoomA].dir_option[NORTH]->exit_info = EX_ISDOOR | EX_CLOSED | EX_LOCKED;
    EXPECT_FALSE(ranger_can_step(&m_mob, NORTH)) << "locked";
}

TEST_F(RangerProgTest, ARefusedDoorThatOpenedEndsTheTurn)
{
    /* The door opens, then the step is refused (water): opening was its
     * action, so it does not also wander off by the other exit. */
    make_room_c();
    m_mob.specials2.act |= MOB_AGGRESSIVE;
    place(&m_player, kRoomB);
    set_options("track=yes");
    world[kRoomB].sector_type = SECT_WATER_NOSWIM;
    world[kRoomA].dir_option[NORTH]->exit_info = EX_ISDOOR | EX_CLOSED;
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    link(kRoomA, EAST, kRoomC);
    for (int i = 0; i < 30; ++i) { /* wander would pick east one turn in six */
        world[kRoomA].dir_option[NORTH]->exit_info = EX_ISDOOR | EX_CLOSED;
        place(&m_mob, kRoomA);
        m_mob.delay.wait_value = 0;
        turn();
        EXPECT_FALSE(IS_SET(world[kRoomA].dir_option[NORTH]->exit_info, EX_CLOSED)) << "the door was opened";
        ASSERT_EQ(m_mob.in_room, kRoomA) << "turn " << i;
    }
}

TEST_F(RangerProgTest, WandersWhenFree)
{
    place(&m_player, kRoomB);
    EXPECT_TRUE(wanders_within(200));
}

TEST_F(RangerProgTest, SentinelDoesNotWander)
{
    m_mob.specials2.act |= MOB_SENTINEL;
    place(&m_player, kRoomB);
    EXPECT_FALSE(wanders_within(200));
}

TEST_F(RangerProgTest, MobWithAMasterDoesNotWander)
{
    place(&m_player, kRoomB);
    m_mob.master = &m_player;
    bool wandered = wanders_within(200);
    m_mob.master = nullptr;
    EXPECT_FALSE(wandered);
}

TEST_F(RangerProgTest, RiddenMountDoesNotWander)
{
    ride();
    bool wandered = wanders_within(200);
    unride();
    EXPECT_FALSE(wandered);
    EXPECT_EQ(m_player.in_room, kRoomA);
}

TEST_F(RangerProgTest, RiddenMountDoesNotFollowTracks)
{
    m_mob.specials2.act |= MOB_SENTINEL;
    m_mob.specials2.pref = 1 << RACE_ORC;
    set_options("track=yes");
    world[kRoomA].room_track[0].char_number = -RACE_ORC;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    ride();
    turn();
    unride();
    EXPECT_EQ(m_mob.in_room, kRoomA);
}

TEST_F(RangerProgTest, RiddenMountDoesNotHunt)
{
    m_mob.specials2.act |= MOB_MEMORY | MOB_HUNTER | MOB_SENTINEL;
    remember(&m_mob, &m_player);
    place(&m_player, kRoomB);
    make_mob(&m_ally, kAllySlot, m_ally_name);
    m_ally.specials.store_prog_number = 0;
    m_ally.specials2.act = MOB_ISNPC;
    ride(&m_ally, kAllySlot);
    turn();
    unride();
    EXPECT_EQ(m_mob.in_room, kRoomA);
}

/* The same rule on the standard mob turn, for a mob with no program. */
TEST_F(RangerProgTest, StandardTurnMountWandersWhenNotRidden)
{
    place(&m_player, kRoomB);
    EXPECT_TRUE(standard_turn_wanders_within(2000));
}

TEST_F(RangerProgTest, StandardTurnHunterStepsWhenNotRidden)
{
    m_mob.specials2.act = MOB_ISNPC | MOB_MEMORY | MOB_HUNTER | MOB_SENTINEL;
    m_mob.specials.store_prog_number = 0;
    remember(&m_mob, &m_player);
    place(&m_player, kRoomB);
    one_mobile_activity(&m_mob);
    EXPECT_EQ(m_mob.in_room, kRoomB);
}

TEST_F(RangerProgTest, StandardTurnRiddenMountDoesNotHunt)
{
    m_mob.specials2.act = MOB_ISNPC | MOB_MEMORY | MOB_HUNTER | MOB_SENTINEL;
    m_mob.specials.store_prog_number = 0;
    remember(&m_mob, &m_player);
    place(&m_player, kRoomB);
    make_mob(&m_ally, kAllySlot, m_ally_name);
    m_ally.specials.store_prog_number = 0;
    m_ally.specials2.act = MOB_ISNPC;
    ride(&m_ally, kAllySlot);
    one_mobile_activity(&m_mob);
    unride();
    EXPECT_EQ(m_mob.in_room, kRoomA);
}

TEST_F(RangerProgTest, StandardTurnRiddenMountDoesNotWander)
{
    ride();
    bool wandered = standard_turn_wanders_within(2000);
    unride();
    EXPECT_FALSE(wandered);
    EXPECT_EQ(m_player.in_room, kRoomA);
}

TEST_F(RangerProgTest, HideOptionStartsToHide)
{
    m_mob.specials2.act |= MOB_SENTINEL;
    place(&m_player, kRoomB);
    turn();
    EXPECT_NE(m_mob.delay.cmd, CMD_HIDE) << "no hide without the option";
    set_options("hide=yes");
    turn();
    EXPECT_EQ(m_mob.delay.cmd, CMD_HIDE);
    abort_delay(&m_mob);
}

TEST_F(RangerProgTest, HelperJoinsAnAllysFight)
{
    m_mob.specials2.act |= MOB_HELPER | MOB_SENTINEL;
    ally_in_a_fight();
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_enemy);
}

TEST_F(RangerProgTest, HelpComesBeforeAPlainHit)
{
    m_mob.specials2.act |= MOB_HELPER | MOB_AGGRESSIVE | MOB_SENTINEL;
    ally_in_a_fight();
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_enemy) << "helps rather than hitting the player";
}

TEST_F(RangerProgTest, AmbushComesBeforeHelp)
{
    m_mob.specials2.act |= MOB_HELPER | MOB_AGGRESSIVE | MOB_SENTINEL;
    set_options("ambush=yes");
    ally_in_a_fight();
    obj_data dagger;
    wield_dagger(&dagger);
    turn();
    m_mob.equipment[WIELD] = nullptr;
    EXPECT_EQ(m_mob.delay.cmd, CMD_AMBUSH) << "ambushes the player who is not fighting";
    EXPECT_EQ(m_mob.specials.fighting, nullptr);
    abort_delay(&m_mob);
}

TEST_F(RangerProgTest, RememberedEnemyComesBeforeHelp)
{
    m_mob.specials2.act |= MOB_HELPER | MOB_MEMORY | MOB_SENTINEL;
    ally_in_a_fight();
    remember(&m_mob, &m_player);
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_player);
}

TEST_F(RangerProgTest, RememberedEnemyIsAmbushedWithTheOption)
{
    m_mob.specials2.act |= MOB_MEMORY | MOB_SENTINEL;
    set_options("ambush=yes");
    remember(&m_mob, &m_player);
    obj_data dagger;
    wield_dagger(&dagger);
    turn();
    m_mob.equipment[WIELD] = nullptr;
    EXPECT_EQ(m_mob.delay.cmd, CMD_AMBUSH);
    abort_delay(&m_mob);
}

TEST_F(RangerProgTest, RiposteSettingReachesTheRunningMob)
{
    EXPECT_FALSE(ranger_settings(&m_mob).riposte);
    set_options("riposte=yes");
    EXPECT_TRUE(ranger_settings(&m_mob).riposte);
    m_mob.specials.store_prog_number = 0;
    EXPECT_FALSE(ranger_settings(&m_mob).riposte) << "only a program-32 mob";
}

} // namespace

TEST_F(RangerProgTest, AnEndedAmbushDoesNotStopTheFallbackHit)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    set_options("ambush=yes");
    /* An earlier ambush has ended: its name stays, its wind-up is over. */
    m_mob.delay.cmd = CMD_AMBUSH;
    m_mob.delay.wait_value = 0;
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_player) << "no weapon, so it hits";
}

TEST_F(RangerProgTest, AHiddenPlayerGetsOneRollPerTurn)
{
    /* From level 40 a roll sees this hide about 29% of the time; rolled once
     * by the ambush scan and again by the hit scan it would be about 50%. */
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    m_mob.player.level = 40;
    set_options("ambush=yes");
    constexpr int kTurns = 400;
    int attacks = 0;
    for (int i = 0; i < kTurns; ++i) {
        GET_HIDING(&m_player) = 107;
        turn();
        if (m_mob.specials.fighting) {
            ++attacks;
            stop_fighting(&m_mob);
            if (m_player.specials.fighting)
                stop_fighting(&m_player);
        }
        if (in_waiting_list(&m_mob) || m_mob.delay.wait_value)
            abort_delay(&m_mob);
    }
    EXPECT_GT(attacks, kTurns * 20 / 100);
    EXPECT_LT(attacks, kTurns * 40 / 100);
}

TEST_F(RangerProgTest, DoesNotWanderIntoADeathRoom)
{
    place(&m_player, kRoomB);
    world[kRoomB].room_flags = DEATH;
    EXPECT_FALSE(wanders_within(200));
}

TEST_F(RangerProgTest, TrackIntoADeathRoomIsNotChosen)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    place(&m_player, kRoomB);
    set_options("track=yes");
    world[kRoomB].room_flags = DEATH;
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    turn();
    EXPECT_EQ(m_mob.in_room, kRoomA);
}

TEST_F(RangerProgTest, MobWithAMasterDoesNotFollowTracks)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    place(&m_player, kRoomB);
    set_options("track=yes");
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    m_mob.master = &m_player;
    turn();
    m_mob.master = nullptr;
    EXPECT_EQ(m_mob.in_room, kRoomA);
}

TEST_F(RangerProgTest, ASneakingMobWithHideKeepsWandering)
{
    set_options("hide=yes");
    place(&m_player, kRoomB);
    SET_BIT(m_mob.specials.affected_by, AFF_SNEAK | AFF_HIDE);
    SET_BIT(m_mob.specials2.hide_flags, HIDING_SNUCK_IN); /* as snuck_in leaves it */
    GET_HIDING(&m_mob) = 50;
    EXPECT_TRUE(wanders_within(200));
}

TEST_F(RangerProgTest, NothingWhileSomethingIsPending)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    m_mob.delay.wait_value = 5; /* a script's wait, say */
    turn();
    EXPECT_EQ(m_mob.specials.fighting, nullptr);
    m_mob.delay.wait_value = 0;
}

TEST_F(RangerProgTest, AHideThatStartsEndsTheTurn)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    set_options("hide=yes");
    turn();
    EXPECT_EQ(m_mob.delay.cmd, CMD_HIDE);
    EXPECT_EQ(m_mob.specials.fighting, nullptr) << "the hide finishes first";
    abort_delay(&m_mob);
    m_mob.delay.wait_value = 0;
    GET_HIDING(&m_mob) = 50; /* the hide is done */
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_player) << "then it attacks from cover";
}

TEST_F(RangerProgTest, ACharmedMobDoesNotAttackItsMaster)
{
    m_mob.specials2.act |= MOB_AGGRESSIVE | MOB_SENTINEL;
    SET_BIT(m_mob.specials.affected_by, AFF_CHARM);
    m_mob.master = &m_player;
    turn();
    m_mob.master = nullptr;
    EXPECT_EQ(m_mob.specials.fighting, nullptr);
    EXPECT_EQ(std::string(m_descriptor.output).find("attacks"), std::string::npos) << m_descriptor.output;
}

TEST_F(RangerProgTest, AHunterWhoseStepIsRefusedGoesOnInTheSameTurn)
{
    /* The enemy is out of the mob's zone: the step toward them is refused,
     * so the same turn goes on to the track east instead of ending. */
    make_room_c();
    m_mob.specials2.act |= MOB_MEMORY | MOB_HUNTER | MOB_STAY_ZONE | MOB_AGGRESSIVE | MOB_SENTINEL;
    set_options("track=yes");
    remember(&m_mob, &m_player);
    place(&m_player, kRoomB);
    world[kRoomB].zone = 1;
    link(kRoomA, EAST, kRoomC);
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + EAST;
    turn();
    EXPECT_EQ(m_mob.in_room, kRoomC);
}

TEST_F(RangerProgTest, AHelperThatCannotSeeTheEnemyGoesOn)
{
    m_mob.specials2.act |= MOB_HELPER | MOB_AGGRESSIVE | MOB_SENTINEL;
    ally_in_a_fight();
    SET_BIT(m_enemy.specials.affected_by, AFF_INVISIBLE);
    turn();
    EXPECT_EQ(m_mob.specials.fighting, &m_player) << "no help possible, so it hits the player it sees";
}

TEST_F(RangerProgTest, AHuntStepIntoWaterItCannotSwimGoesOnInTheSameTurn)
{
    /* The checks before the step do not know about water; the step is
     * refused, the mob has not moved, so the turn goes on to the track. */
    make_room_c();
    m_mob.specials2.act |= MOB_MEMORY | MOB_HUNTER | MOB_AGGRESSIVE | MOB_SENTINEL;
    set_options("track=yes");
    remember(&m_mob, &m_player);
    place(&m_player, kRoomB);
    world[kRoomB].sector_type = SECT_WATER_NOSWIM;
    link(kRoomA, EAST, kRoomC);
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + EAST;
    turn();
    EXPECT_EQ(m_mob.in_room, kRoomC);
}

TEST_F(RangerProgTest, TrackThroughALeverDoorIsNotChosen)
{
    /* A lever door does not open by hand ("It will not budge"), so the track
     * is not chosen at all; a plain closed door is. */
    m_mob.specials2.act |= MOB_AGGRESSIVE;
    place(&m_player, kRoomB);
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    world[kRoomA].dir_option[NORTH]->exit_info = EX_ISDOOR | EX_CLOSED;
    EXPECT_EQ(ranger_track_direction(&m_mob), NORTH);
    world[kRoomA].dir_option[NORTH]->exit_info = EX_ISDOOR | EX_CLOSED | EX_LEVER;
    EXPECT_EQ(ranger_track_direction(&m_mob), -1);
}

TEST_F(RangerProgTest, AStepThatIsRefusedLetsTheTrackGoOnToWander)
{
    /* The track leads into water the mob cannot swim: refused every time, so
     * each turn goes on to wander, and the mob gets away by the other exit. */
    make_room_c();
    m_mob.specials2.act |= MOB_AGGRESSIVE;
    place(&m_player, kRoomB);
    set_options("track=yes");
    world[kRoomB].sector_type = SECT_WATER_NOSWIM;
    world[kRoomA].room_track[0].char_number = -RACE_HUMAN;
    world[kRoomA].room_track[0].data = time_info.hours * 8 + NORTH;
    link(kRoomA, EAST, kRoomC);
    EXPECT_TRUE(wanders_within(200));
    EXPECT_EQ(m_mob.in_room, kRoomC);
}

TEST_F(RangerProgTest, AnAssistThatSanctuaryStopsStillEndsTheTurn)
{
    /* It said its line and assisted: that is its action, even though the
     * enemy's sanctuary stopped the blow. It does not also hit the player. */
    m_mob.specials2.act |= MOB_HELPER | MOB_AGGRESSIVE | MOB_SENTINEL;
    ally_in_a_fight();
    affected_type sanctuary {};
    sanctuary.type = SPELL_SANCTUARY;
    sanctuary.duration = 10;
    sanctuary.modifier = 1000000; /* never wears through in this test */
    affect_to_char(&m_enemy, &sanctuary);
    turn();
    EXPECT_NE(m_mob.specials.fighting, &m_player);
    affect_from_char(&m_enemy, SPELL_SANCTUARY);
}
