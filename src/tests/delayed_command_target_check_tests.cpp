#include "../handler.h"
#include "../interpre.h"
#include "../spells.h"
#include "../structs.h"

#include <gtest/gtest.h>

#include <string>

extern struct room_data world;
// interpre.cpp's generic target check and the builder of the cmd_info[] masks it reads; no
// header declares either.
int target_check(struct char_data* ch, int cmd, const struct target_data* t1,
    const struct target_data* t2);
void assign_command_pointers(void);

namespace {

// The caster's lit room, and the room its target stands in: DARK and unlit unless a test lights it.
constexpr int kCasterRoom = 975;
constexpr int kTargetRoom = 976;
// The char_exists() slots of the caster and the target, which no other suite uses.
constexpr int kCasterSlot = 4760;
constexpr int kTargetSlot = 4761;

// A human player whose cast at a human target has reached the end of its delay. The target stands
// in kTargetRoom, which the caster cannot see into while it is dark.
class DelayedCastTargetCheck : public testing::Test {
protected:
    void SetUp() override
    {
        assign_command_pointers();
        m_saved_caster_room = world[kCasterRoom];
        m_saved_target_room = world[kTargetRoom];
        prepare_room(kCasterRoom, 0, 1);
        prepare_room(kTargetRoom, DARK, 0);

        prepare_character(m_caster, kCasterSlot, m_caster_name, kCasterRoom);
        prepare_character(m_target, kTargetSlot, m_target_name, kTargetRoom);
        m_descriptor.output = m_descriptor.small_outbuf;
        m_descriptor.small_outbuf[0] = '\0';
        m_descriptor.bufptr = 0;
        m_descriptor.bufspace = SMALL_BUFSIZE - 1;
        m_descriptor.connected = CON_PLYNG;
        m_caster.desc = &m_descriptor;
    }

    void TearDown() override
    {
        m_delay.targ1.cleanup();
        m_delay.targ2.cleanup();
        remove_char_exists(kCasterSlot);
        remove_char_exists(kTargetSlot);
        world[kTargetRoom] = m_saved_target_room;
        world[kCasterRoom] = m_saved_caster_room;
    }

    // Empties room and gives it room_flags and light. The room's sector is SECT_INSIDE, which
    // IS_DARK never darkens for night, so only room_flags and light decide whether it is dark.
    static void prepare_room(int room, long room_flags, int light)
    {
        world[room].room_flags = room_flags;
        world[room].light = light;
        world[room].sector_type = SECT_INSIDE;
        world[room].people = nullptr;
        world[room].contents = nullptr;
    }

    // Makes character a standing level-30 human player named name, registered in slot and
    // standing in room. The fixture never links it into the room's people list: the re-check
    // reads only in_room.
    static void prepare_character(char_data& character, int slot, char* name, int room)
    {
        character.abs_number = slot;
        set_char_exists(slot);
        character.player.name = name;
        character.player.race = RACE_HUMAN;
        character.player.level = 30;
        character.specials.position = POSITION_STANDING;
        character.in_room = room;
    }

    // Stores the delay do_cast leaves for a cast of spell_index at the target, which the cast's
    // parse found as target_choice.
    void delay_cast_at_target(int spell_index, int target_choice)
    {
        m_delay.cmd = CMD_CAST;
        m_delay.subcmd = spell_index;
        m_delay.targ1.type = TARGET_OTHER;
        m_delay.targ1.ch_num = spell_index;
        m_delay.targ2.set_character(&m_target);
        m_delay.targ2.choice = target_choice;
    }

    // Runs the re-check command_interpreter makes when the caster's delay ends.
    int recheck() { return delayed_command_target_check(&m_caster, &m_delay); }

    // What the re-check told the caster.
    std::string caster_output() const { return m_descriptor.output; }

    // Empties what the caster has been told.
    void clear_caster_output()
    {
        m_descriptor.small_outbuf[0] = '\0';
        m_descriptor.bufptr = 0;
        m_descriptor.bufspace = SMALL_BUFSIZE - 1;
    }

    // The caster's name, which player.name points at.
    char m_caster_name[16] = "Caster";
    // The target's name, which player.name points at.
    char m_target_name[16] = "Target";
    // The character whose delay is re-checked.
    char_data m_caster {};
    // The character the delayed cast is aimed at.
    char_data m_target {};
    // The caster's connection; its small buffer collects what the re-check sends.
    descriptor_data m_descriptor {};
    // The delayed command under test, as the caster's delay field would hold it.
    waiting_type m_delay {};
    // The caster's room as SetUp found it; TearDown puts it back.
    room_data m_saved_caster_room {};
    // The target's room as SetUp found it; TearDown puts it back.
    room_data m_saved_target_room {};
};

} // namespace

TEST_F(DelayedCastTargetCheck, SummonAtAPlayerStandingInADarkRoomPassesTheReCheck)
{
    delay_cast_at_target(SPELL_SUMMON, TAR_CHAR_WORLD);

    EXPECT_EQ(recheck(), 1) << "The caster read: " << caster_output();
    EXPECT_EQ(caster_output(), "");
}

// The refusal is logged as a SYSERR and the caster is told nothing.
TEST_F(DelayedCastTargetCheck, ANullDelayedCommandIsRefused)
{
    testing::internal::CaptureStderr();
    const int accepted = delayed_command_target_check(&m_caster, nullptr);
    const std::string logged = testing::internal::GetCapturedStderr();

    EXPECT_EQ(accepted, 0);
    EXPECT_EQ(caster_output(), "");
    EXPECT_NE(logged.find("SYSERR: delayed_command_target_check"), std::string::npos) << logged;
}

// The refusal is logged as a SYSERR.
TEST_F(DelayedCastTargetCheck, ANullCharacterIsRefused)
{
    delay_cast_at_target(SPELL_SUMMON, TAR_CHAR_WORLD);

    testing::internal::CaptureStderr();
    const int accepted = delayed_command_target_check(nullptr, &m_delay);
    const std::string logged = testing::internal::GetCapturedStderr();

    EXPECT_EQ(accepted, 0);
    EXPECT_NE(logged.find("SYSERR: delayed_command_target_check"), std::string::npos) << logged;
}

TEST_F(DelayedCastTargetCheck, ASpellWithoutDarkOkIsRefusedATargetInTheDark)
{
    delay_cast_at_target(SPELL_FIREBALL, TAR_CHAR_ROOM);

    EXPECT_EQ(recheck(), 0);
    EXPECT_EQ(caster_output(), "Nobody here by that name.\n\r");
}

// The re-check leaves a room target that walked out to do_cast, which says "Your victim has fled."
TEST_F(DelayedCastTargetCheck, ARoomTargetThatLeftForALitRoomPassesTheReCheck)
{
    world[kTargetRoom].light = 1;
    delay_cast_at_target(SPELL_FIREBALL, TAR_CHAR_ROOM);

    EXPECT_EQ(recheck(), 1) << "The caster read: " << caster_output();
}

// The re-check leaves an opponent that walked out to do_cast, which says "You could not find
// your opponent."
TEST_F(DelayedCastTargetCheck, AFightVictimThatLeftForALitRoomPassesTheReCheck)
{
    world[kTargetRoom].light = 1;
    delay_cast_at_target(SPELL_CONFUSE, TAR_FIGHT_VICT);

    EXPECT_EQ(recheck(), 1) << "The caster read: " << caster_output();
}

// The re-check leaves an object no longer carried to do_cast, which says "Your target disappeared."
TEST_F(DelayedCastTargetCheck, AnObjectNoLongerCarriedPassesTheReCheck)
{
    obj_data dropped_item {};
    dropped_item.in_room = NOWHERE;
    dropped_item.carried_by = nullptr;
    delay_cast_at_target(SPELL_IDENTIFY, TAR_OBJ_INV);
    m_delay.targ2.type = TARGET_OBJ;
    m_delay.targ2.ptr.obj = &dropped_item;
    m_delay.targ2.ch_num = 0;

    EXPECT_EQ(recheck(), 1) << "The caster read: " << caster_output();
}

TEST_F(DelayedCastTargetCheck, ARoomCastWithNoTargetPassesTheReCheck)
{
    delay_cast_at_target(SPELL_BLAZE, TAR_IGNORE);
    m_delay.targ2.cleanup();
    m_delay.targ2.choice = TAR_IGNORE;

    EXPECT_EQ(recheck(), 1) << "The caster read: " << caster_output();
}

// A subcmd of -1 marks an interrupted cast rather than a spell index.
TEST_F(DelayedCastTargetCheck, AnInterruptedCastIsRefusedATargetInTheDark)
{
    delay_cast_at_target(SPELL_SUMMON, TAR_CHAR_WORLD);
    m_delay.subcmd = -1;

    EXPECT_EQ(recheck(), 0);
    EXPECT_EQ(caster_output(), "Nobody by that name.\n\r");
}

// A non-cast command's subcmd is not a spell index, even when it equals summon's, and its
// re-check is target_check()'s, result and message alike.
TEST_F(DelayedCastTargetCheck, ANonCastCommandIsRefusedATargetInTheDark)
{
    delay_cast_at_target(SPELL_SUMMON, TAR_CHAR_WORLD);
    m_delay.cmd = CMD_KILL;
    m_delay.targ1.set_character(&m_target);
    m_delay.targ1.choice = TAR_CHAR_ROOM;
    m_delay.targ2.cleanup();

    const int delayed_result = recheck();
    const std::string delayed_output = caster_output();
    clear_caster_output();
    const int generic_result = target_check(&m_caster, CMD_KILL, &m_delay.targ1, &m_delay.targ2);

    EXPECT_EQ(delayed_result, 0);
    EXPECT_EQ(delayed_output, "Nobody here by that name.\n\r");
    EXPECT_EQ(delayed_result, generic_result);
    EXPECT_EQ(delayed_output, caster_output());
}
