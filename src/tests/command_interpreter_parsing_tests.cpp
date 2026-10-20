#include "../structs.h"
#include "../db.h"
#include "../input_command_list.h"
#include "../input_command_parser.h"
#include "../interpre.h"
#include "../utils.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

/*
 * How command_interpreter turns a typed line into a command and the text its handler receives.
 * The fixture isolates that from the rest of the game: every command the tests use runs a
 * recording handler, target parsing accepts any text, specials are off, and the socials are a
 * three-entry list, so no world or data files are loaded.
 */

extern command_info cmd_info[];
extern social_messg* soc_mess_list;
extern int social_list_top;
extern int no_specials;
extern char* fill[];
void clear_char(struct char_data* ch, int mode);
int old_search_block(const char* argument, int begin, unsigned int length, const char** list,
    int mode);

namespace {

// What the last command handler run by command_interpreter received.
struct recorded_command {
    // How many handlers ran since the recording was cleared.
    int calls = 0;
    // The command number the handler was called for.
    int cmd = 0;
    // The subcommand stored with the command being run; for CMD_SOCIAL it is the social's index,
    // which the handler's own subcmd argument does not carry.
    int waiting_subcmd = 0;
    // The argument text the handler was given.
    std::string text;
};

// Written by record_command, which command_info can only hold as a plain function pointer.
recorded_command last_command;

void record_command(char_data* character, char* argument, waiting_type* wait_list, int cmd,
    int subcmd)
{
    (void)character;
    (void)subcmd;
    ++last_command.calls;
    last_command.cmd = cmd;
    last_command.waiting_subcmd = wait_list->subcmd;
    last_command.text = argument;
}

// The fixture's socials in the sorted order find_action searches: bow, smile, twitch.
char bow_name[] = "bow";
char smile_name[] = "smile";
char twitch_name[] = "twitch";

constexpr int MIDDLE_SOCIAL = 1; // smile
constexpr int TWITCH_SOCIAL = 2;

} // namespace

class CommandInterpreterParsingTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::copy(cmd_info, cmd_info + MAX_CMD_LIST, saved_commands.begin());
        std::fill(cmd_info, cmd_info + MAX_CMD_LIST, command_info {});

        saved_socials = soc_mess_list;
        saved_social_list_top = social_list_top;
        socials[0].command = bow_name;
        socials[1].command = smile_name;
        socials[2].command = twitch_name;
        soc_mess_list = socials.data();
        social_list_top = static_cast<int>(socials.size()) - 1;

        saved_no_specials = no_specials;
        no_specials = 1;

        clear_char(&character, 0);
        character.player.level = 10;
        GET_POS(&character) = POSITION_STANDING;
        descriptor.output = descriptor.small_outbuf;
        descriptor.connected = CON_PLYNG;
        character.desc = &descriptor;
        clear_output();

        last_command = recorded_command {};
    }

    void TearDown() override
    {
        std::copy(saved_commands.begin(), saved_commands.end(), cmd_info);
        soc_mess_list = saved_socials;
        social_list_top = saved_social_list_top;
        no_specials = saved_no_specials;
    }

    // Makes a command run the recording handler, accepting any text as its target.
    void install(int command_number, int minimum_level = 0, bool retired_allowed = true,
        unsigned char mask = 0)
    {
        command_info& entry = cmd_info[command_number];
        entry.command_pointer = record_command;
        entry.minimum_position = POSITION_RESTING;
        entry.minimum_level = static_cast<sh_int>(minimum_level);
        entry.retired_allowed = retired_allowed;
        entry.mask = mask;
        entry.add_target(TAR_IGNORE, TAR_IGNORE);
    }

    // The number of the command named exactly name in the game's command table.
    static int command_number(const char* name)
    {
        return old_search_block(name, 0, std::strlen(name), const_cast<const char**>(COMMAND_NAMES), 1);
    }

    void allow_shaping_at_immortal_level()
    {
        cmd_info[CMD_SHAPE].minimum_level = LEVEL_IMMORT;
    }

    // Types a line as the character, from a buffer the size callers pass.
    void type(const char* line)
    {
        std::strncpy(line_buffer, line, sizeof(line_buffer) - 1);
        line_buffer[sizeof(line_buffer) - 1] = '\0';
        clear_output();
        last_command = recorded_command {};
        command_interpreter(&character, line_buffer);
    }

    void clear_output()
    {
        descriptor.small_outbuf[0] = '\0';
        descriptor.bufptr = 0;
        descriptor.bufspace = SMALL_BUFSIZE - 1;
    }

    std::string output() const
    {
        return std::string(descriptor.output);
    }

    bool is_hidden() const
    {
        return IS_SET(character.specials.affected_by, AFF_HIDE);
    }

    void expect_unrecognized() const
    {
        EXPECT_EQ(last_command.calls, 0);
        EXPECT_NE(output().find("Unrecognized command."), std::string::npos) << output();
    }

    void expect_shape_center() const
    {
        EXPECT_EQ(last_command.calls, 0);
        EXPECT_NE(output().find("Use 'shape <mob|obj|room|script> <number>' to start shaping."),
            std::string::npos)
            << output();
    }

    // The command table as the rest of the suite expects it, restored after each test.
    std::array<command_info, MAX_CMD_LIST> saved_commands;
    // The socials list find_action searches during a test.
    std::array<social_messg, 3> socials {};
    // The game's socials list, restored after each test.
    social_messg* saved_socials = nullptr;
    // The game's last socials index, restored after each test.
    int saved_social_list_top = -1;
    // The game's specials switch, restored after each test.
    int saved_no_specials = 0;
    // The player typing the lines.
    char_data character {};
    // The player's connection; its output buffer holds what the game sent them.
    descriptor_data descriptor {};
    // The typed line as command_interpreter receives it.
    char line_buffer[MAX_INPUT_LENGTH] = "";
};

TEST_F(CommandInterpreterParsingTest, SkipsLeadingSpacesBeforeTheCommandWord)
{
    install(CMD_LOOK);

    type("   look  at sky");

    EXPECT_EQ(last_command.calls, 1);
    EXPECT_EQ(last_command.cmd, CMD_LOOK);
    EXPECT_EQ(last_command.text, "  at sky");
}

TEST_F(CommandInterpreterParsingTest, HandsTheHandlerEverythingAfterTheCommandWord)
{
    install(CMD_LOOK);

    type("look x");

    EXPECT_EQ(last_command.text, " x");
}

TEST_F(CommandInterpreterParsingTest, LowercasesOnlyTheCommandWord)
{
    install(CMD_LOOK);

    type("LOOK X");

    EXPECT_EQ(last_command.cmd, CMD_LOOK);
    EXPECT_EQ(last_command.text, " X");
}

TEST_F(CommandInterpreterParsingTest, MatchesTheFirstCommandInTableOrder)
{
    const int north = command_number("north");
    const int south = command_number("south");
    install(north);
    install(south);

    type("n");
    EXPECT_EQ(last_command.cmd, north);

    type("s");
    EXPECT_EQ(last_command.cmd, south);
}

TEST_F(CommandInterpreterParsingTest, KeepsAnAccentedByteInTheCommandWord)
{
    install(CMD_LOOK);

    type("look\xE9");

    expect_unrecognized();
}

TEST_F(CommandInterpreterParsingTest, IgnoresABlankLine)
{
    install(CMD_LOOK);

    type("   ");

    EXPECT_EQ(last_command.calls, 0);
    EXPECT_EQ(output(), "");
}

TEST_F(CommandInterpreterParsingTest, DoesNotSkipALeadingTab)
{
    install(CMD_LOOK);

    type("\tlook");

    expect_unrecognized();
}

TEST_F(CommandInterpreterParsingTest, TurnsAQuoteIntoSay)
{
    install(CMD_SAY);

    type("'hello");

    EXPECT_EQ(last_command.cmd, CMD_SAY);
    EXPECT_EQ(last_command.text, " hello");
}

TEST_F(CommandInterpreterParsingTest, TurnsASemicolonIntoWiznet)
{
    install(CMD_WIZNET);

    type(";hi");

    EXPECT_EQ(last_command.cmd, CMD_WIZNET);
    EXPECT_EQ(last_command.text, " hi");
}

TEST_F(CommandInterpreterParsingTest, TurnsACommaIntoEmote)
{
    install(CMD_EMOTE);

    type(",waves");

    EXPECT_EQ(last_command.cmd, CMD_EMOTE);
    EXPECT_EQ(last_command.text, " waves");
}

TEST_F(CommandInterpreterParsingTest, HandsALoneShortcutASingleSpace)
{
    install(CMD_SAY);

    type("'");

    EXPECT_EQ(last_command.cmd, CMD_SAY);
    EXPECT_EQ(last_command.text, " ");
}

TEST_F(CommandInterpreterParsingTest, RunsTheShapeShortcutAtShapeLevel)
{
    allow_shaping_at_immortal_level();
    character.player.level = LEVEL_IMMORT;

    type("/mob 1");

    expect_shape_center();
}

TEST_F(CommandInterpreterParsingTest, TreatsTheShapeShortcutBelowShapeLevelAsAWord)
{
    allow_shaping_at_immortal_level();

    type("/mob");

    expect_unrecognized();
}

TEST_F(CommandInterpreterParsingTest, TreatsTheShapeShortcutWhenRetiredAsAWord)
{
    allow_shaping_at_immortal_level();
    character.player.level = LEVEL_IMMORT;
    SET_BIT(PLR_FLAGS(&character), PLR_RETIRED);

    type("/mob");

    expect_unrecognized();
}

TEST_F(CommandInterpreterParsingTest, SendsAShapingCharactersLineToShapeCenter)
{
    allow_shaping_at_immortal_level();
    character.player.level = LEVEL_IMMORT;
    SET_BIT(PLR_FLAGS(&character), PLR_RETIRED);
    GET_POS(&character) = POSITION_SHAPING;
    install(CMD_LOOK);

    type("  look");

    expect_shape_center();
}

TEST_F(CommandInterpreterParsingTest, FallsBackToSocialsForACommandAboveTheCharactersLevel)
{
    install(command_number("at"), LEVEL_GOD + 1);
    install(CMD_SOCIAL);

    type("at");

    EXPECT_EQ(last_command.cmd, CMD_SOCIAL);
    EXPECT_EQ(last_command.waiting_subcmd, MIDDLE_SOCIAL);
    EXPECT_EQ(last_command.text, "");
}

TEST_F(CommandInterpreterParsingTest, FallsBackToSocialsForACommandARetiredCharacterCannotUse)
{
    install(command_number("smash"), 0, false);
    install(CMD_SOCIAL);
    SET_BIT(PLR_FLAGS(&character), PLR_RETIRED);

    type("sm");

    EXPECT_EQ(last_command.cmd, CMD_SOCIAL);
    EXPECT_EQ(last_command.waiting_subcmd, MIDDLE_SOCIAL);
}

TEST_F(CommandInterpreterParsingTest, ReportsAnUnknownWordWithNoSocial)
{
    type("xyzzy");

    expect_unrecognized();
}

TEST_F(CommandInterpreterParsingTest, ReadsTheSocialWordPastFillWords)
{
    install(CMD_SOCIAL);

    type("the smile");

    EXPECT_EQ(last_command.cmd, CMD_SOCIAL);
    EXPECT_EQ(last_command.waiting_subcmd, MIDDLE_SOCIAL);
    EXPECT_EQ(last_command.text, " smile");
}

TEST_F(CommandInterpreterParsingTest, TreatsUppercaseFillWordsAsFillWords)
{
    install(CMD_SOCIAL);

    type("THE TWITCH");

    EXPECT_EQ(last_command.waiting_subcmd, TWITCH_SOCIAL);
}

TEST_F(CommandInterpreterParsingTest, LooksUpASocialAbbreviationWithFindAction)
{
    install(CMD_SOCIAL);

    type("twi");

    EXPECT_EQ(last_command.cmd, CMD_SOCIAL);
    EXPECT_EQ(last_command.waiting_subcmd, TWITCH_SOCIAL);
}

TEST_F(CommandInterpreterParsingTest, PerformsTheMiddleSocialForAFillWordAlone)
{
    install(CMD_SOCIAL);

    type("from");

    EXPECT_EQ(last_command.cmd, CMD_SOCIAL);
    EXPECT_EQ(last_command.waiting_subcmd, MIDDLE_SOCIAL);
}

TEST_F(CommandInterpreterParsingTest, ReadsAQuotedSocialWordAsThePreviousFillWordsFirstLetter)
{
    install(CMD_SOCIAL);

    type("the 'bob");

    EXPECT_EQ(last_command.cmd, CMD_SOCIAL);
    EXPECT_EQ(last_command.waiting_subcmd, TWITCH_SOCIAL) << "t finds twitch";
    EXPECT_EQ(last_command.text, " 'bob");
}

TEST_F(CommandInterpreterParsingTest, KeepsAPractiseWithNothingAfterItHidden)
{
    install(CMD_PRACTISE);
    install(CMD_PRACTICE);

    SET_BIT(character.specials.affected_by, AFF_HIDE);
    type("practise");
    EXPECT_EQ(last_command.cmd, CMD_PRACTISE);
    EXPECT_TRUE(is_hidden());

    type("practice");
    EXPECT_EQ(last_command.cmd, CMD_PRACTICE);
    EXPECT_TRUE(is_hidden());
}

TEST_F(CommandInterpreterParsingTest, UnhidesAPractiseWithATrailingSpace)
{
    install(CMD_PRACTISE);
    SET_BIT(character.specials.affected_by, AFF_HIDE);

    type("practise ");

    EXPECT_EQ(last_command.cmd, CMD_PRACTISE);
    EXPECT_FALSE(is_hidden());
}

TEST_F(CommandInterpreterParsingTest, UnhidesForAnOrdinaryCommand)
{
    install(CMD_LOOK);
    SET_BIT(character.specials.affected_by, AFF_HIDE);

    type("look x");

    EXPECT_FALSE(is_hidden());
}

TEST_F(CommandInterpreterParsingTest, HandsOverALineThatFillsTheBuffer)
{
    install(CMD_LOOK);
    const std::string word = "look";
    const std::string tail(MAX_INPUT_LENGTH - 1 - word.size() - 1, 'x');
    const std::string line = word + " " + tail;
    ASSERT_EQ(line.size(), static_cast<std::size_t>(MAX_INPUT_LENGTH - 1));

    type(line.c_str());

    EXPECT_EQ(last_command.text, " " + tail);
}

TEST_F(CommandInterpreterParsingTest, IgnoresANullLine)
{
    install(CMD_LOOK);

    command_interpreter(&character, nullptr);

    EXPECT_EQ(last_command.calls, 0);
    EXPECT_EQ(output(), "");
}

TEST_F(CommandInterpreterParsingTest, CutsALongerLineToTheBuffer)
{
    install(CMD_LOOK);
    const std::string word = "look";
    const std::string tail(2 * MAX_INPUT_LENGTH, 'x');
    std::string line = word + " " + tail;

    command_interpreter(&character, line.data());

    const std::string expected_tail(MAX_INPUT_LENGTH - 1 - word.size() - 1, 'x');
    EXPECT_EQ(last_command.text, " " + expected_tail);
}

TEST_F(CommandInterpreterParsingTest, PerformsTheMiddleSocialForABareQuotedWord)
{
    install(CMD_SAY, LEVEL_GOD);
    install(CMD_SOCIAL);

    type("''x");

    EXPECT_EQ(last_command.cmd, CMD_SOCIAL);
    EXPECT_EQ(last_command.waiting_subcmd, MIDDLE_SOCIAL);
    EXPECT_EQ(last_command.text, " 'x");
}

// The social word must skip exactly the fill words the game's other commands skip, so a change to
// either list fails here until both agree.
TEST(SocialWordFillWords, MatchTheGamesFillWordTable)
{
    std::vector<std::string> game_fill_words;
    for (int index = 0; fill[index][0] != '\n'; ++index) {
        game_fill_words.emplace_back(fill[index]);
    }
    const std::vector<std::string> expected_fill_words
        = { "in", "from", "with", "the", "on", "at", "to" };
    EXPECT_EQ(game_fill_words, expected_fill_words);

    for (const std::string& fill_word : game_fill_words) {
        const std::string line = fill_word + " smile";
        EXPECT_EQ(read_social_word(line), "smile") << fill_word << " is not skipped";
    }
    EXPECT_EQ(read_social_word("tiny smile"), "tiny") << "a word outside the table is skipped";
}
