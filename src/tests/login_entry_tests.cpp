#include "account_management.h"
#include "boards.h"
#include "db.h"
#include "handler.h"
#include "interpre.h"
#include "objects_json.h"
#include "protocol.h"
#include "structs.h"
#include "utils.h"
#include "zone.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

extern struct ban_list_element* ban_list;
extern struct txt_block* bufpool;
extern struct char_data* character_list;
extern struct descriptor_data* descriptor_list;
extern char* imotd;
extern char* motd;
extern struct board_info_type* news_board;
extern struct player_index_element* player_table;
extern int restrict;
extern int top_of_p_table;
extern int top_of_world;
extern char* wizlock_msg;
extern struct room_data world;
extern struct zone_data* zone_table;
void clear_char(struct char_data* ch, int mode);
int create_entry(const char* name);

namespace {

// The account every test logs in to.
constexpr char ACCOUNT_NAME[] = "acct";
// That account's email address, as the account menu shows it.
constexpr char ACCOUNT_EMAIL[] = "player@example.com";
// That account's password.
constexpr char ACCOUNT_PASSWORD[] = "ValidPass1";
// When the account was created; any fixed time will do.
constexpr long ACCOUNT_CREATED_AT = 1700010200;
// When characters are linked to the account.
constexpr long CHARACTER_LINKED_AT = 1700010201;
// The address the connection logging in comes from.
constexpr char CONNECTION_HOST[] = "127.0.0.1";
// A socket number no write reaches. The login code sends telnet negotiation straight to the
// socket, and it skips the room look for a connection whose socket number is zero.
constexpr int NO_SOCKET = -1;

// The room test characters are saved in, and the index the test world gives it.
constexpr int LOAD_ROOM_VNUM = 3001;
constexpr int LOAD_ROOM_RNUM = 1;
// A second room ahead of it in the world, so the load room's index differs from zero.
constexpr int OTHER_ROOM_VNUM = 1000;
// The load room's name, which the room look on entering the game shows.
constexpr char LOAD_ROOM_NAME[] = "The Testing Meadow";

// The message of the day each kind of character is shown.
constexpr std::string_view MORTAL_MOTD = "Mortal message of the day.\n\r";
constexpr std::string_view IMMORTAL_MOTD = "Immortal message of the day.\n\r";
// The wizlock message an immortal set.
constexpr std::string_view WIZLOCK_MESSAGE = "Closed for testing.\n\r";

// The character menu an account-backed character is shown once selected.
constexpr std::string_view ACCOUNT_CHARACTER_MENU = "\n\r"
                                                    "Welcome to Arda!\n\r"
                                                    "0) Back to Account Menu.\n\r"
                                                    "1) Enter the game.\n\r"
                                                    "3) Read the background story.\n\r"
                                                    "4) Change password.\n\r"
                                                    "5) Delete this character.\n\r"
                                                    "6) View class powers.\n\r"
                                                    "\n\r"
                                                    "   Make your choice: ";

// The character menu a character logged in without an account is shown.
constexpr std::string_view LEGACY_CHARACTER_MENU = "\n\r"
                                                   "Welcome to Arda!\n\r"
                                                   "0) Exit from the MUD.\n\r"
                                                   "1) Enter the game.\n\r"
                                                   "3) Read the background story.\n\r"
                                                   "4) Change password.\n\r"
                                                   "5) Delete this character.\n\r"
                                                   "6) View class powers.\n\r"
                                                   "\n\r"
                                                   "   Make your choice: ";

// The password of the character logged in without an account. The game compares passwords in
// plain text.
constexpr char LEGACY_PASSWORD[] = "LegacyPw1";

// The last words of the account roster's prompt.
constexpr std::string_view ROSTER_PROMPT_END = "Character number or name: ";

// Replaces a global for the life of the scope and restores its old value afterwards.
template <typename Value>
class ScopedGlobal {
public:
    ScopedGlobal(Value& global, Value replacement)
        : target(global)
        , saved_value(global)
    {
        target = replacement;
    }

    ~ScopedGlobal()
    {
        target = saved_value;
    }

    ScopedGlobal(const ScopedGlobal&) = delete;
    ScopedGlobal& operator=(const ScopedGlobal&) = delete;

private:
    // The global this scope replaces.
    Value& target;
    // The global's value before the scope, restored when the scope ends.
    Value saved_value;
};

// A new, empty directory under the system's temporary directory that is the working directory
// for the life of the scope. The game reads and writes account and player files relative to it.
class TemporaryWorkingDirectory {
public:
    TemporaryWorkingDirectory()
    {
        std::error_code error;
        previous_directory = std::filesystem::current_path(error);
        if (error) {
            ADD_FAILURE() << "Could not read the working directory: " << error.message();
            return;
        }

        const std::filesystem::path temporary_root = std::filesystem::temp_directory_path(error);
        if (error) {
            ADD_FAILURE() << "Could not find the temporary directory: " << error.message();
            return;
        }

        std::random_device random_source;
        const std::string directory_name = "rots-login-entry-" + std::to_string(random_source())
            + "-" + std::to_string(random_source());
        directory = temporary_root / directory_name;
        if (!std::filesystem::create_directory(directory, error)) {
            ADD_FAILURE() << "Could not create " << directory << ": " << error.message();
            directory.clear();
            return;
        }

        std::filesystem::current_path(directory, error);
        if (error) {
            ADD_FAILURE() << "Could not enter " << directory << ": " << error.message();
        }
    }

    ~TemporaryWorkingDirectory()
    {
        std::error_code error;
        if (!previous_directory.empty()) {
            std::filesystem::current_path(previous_directory, error);
        }
        if (!directory.empty()) {
            std::filesystem::remove_all(directory, error);
        }
    }

    TemporaryWorkingDirectory(const TemporaryWorkingDirectory&) = delete;
    TemporaryWorkingDirectory& operator=(const TemporaryWorkingDirectory&) = delete;

private:
    // The working directory before the scope, restored when it ends.
    std::filesystem::path previous_directory;
    // The directory this scope created and removes again; empty if it could not be created.
    std::filesystem::path directory;
};

// An empty player table for the life of the scope. Entries the game adds are freed afterwards.
class ScopedEmptyPlayerTable {
public:
    ScopedEmptyPlayerTable()
        : saved_table(player_table)
        , saved_top(top_of_p_table)
    {
        player_table = nullptr;
        top_of_p_table = -1;
    }

    ~ScopedEmptyPlayerTable()
    {
        for (int index = 0; index <= top_of_p_table; ++index) {
            std::free(player_table[index].name);
        }
        std::free(player_table);
        player_table = saved_table;
        top_of_p_table = saved_top;
    }

    ScopedEmptyPlayerTable(const ScopedEmptyPlayerTable&) = delete;
    ScopedEmptyPlayerTable& operator=(const ScopedEmptyPlayerTable&) = delete;

private:
    // The player table before the scope, restored when it ends.
    player_index_element* saved_table;
    // The index of its last entry.
    int saved_top;
};

// A two-room world in one zone for the life of the scope: another room, then the load room. The
// rooms are lit and empty. The suite's world is shared, so both rooms are restored afterwards.
class ScopedTestRooms {
public:
    ScopedTestRooms()
        : saved_top_of_world(top_of_world)
        , saved_first_room(world[0])
        , saved_load_room(world[LOAD_ROOM_RNUM])
        , saved_zone_table(zone_table)
    {
        zone_table = &zone;
        prepare_room(world[0], OTHER_ROOM_VNUM, other_room_name, other_room_description);
        prepare_room(world[LOAD_ROOM_RNUM], LOAD_ROOM_VNUM, load_room_name, load_room_description);
        top_of_world = LOAD_ROOM_RNUM;
    }

    ~ScopedTestRooms()
    {
        world[0] = saved_first_room;
        world[LOAD_ROOM_RNUM] = saved_load_room;
        top_of_world = saved_top_of_world;
        zone_table = saved_zone_table;
    }

    ScopedTestRooms(const ScopedTestRooms&) = delete;
    ScopedTestRooms& operator=(const ScopedTestRooms&) = delete;

private:
    // Makes room an empty, lit room with no exits.
    static void prepare_room(room_data& room, int vnum, std::string& name, std::string& description)
    {
        room.number = vnum;
        room.zone = 0;
        room.sector_type = SECT_INSIDE;
        room.name = name.data();
        room.description = description.data();
        room.ex_description = nullptr;
        for (room_direction_data*& exit : room.dir_option) {
            exit = nullptr;
        }
        room.room_flags = 0;
        room.light = 1;
        room.funct = nullptr;
        room.contents = nullptr;
        room.people = nullptr;
        room.affected = nullptr;
    }

    // The highest room index before the scope.
    int saved_top_of_world;
    // The shared rooms this scope overwrites, restored when it ends.
    room_data saved_first_room;
    room_data saved_load_room;
    // The zone table before the scope.
    zone_data* saved_zone_table;
    // The one zone both rooms belong to; a character entering a room adds to its zone's power.
    zone_data zone { };
    // The rooms' text, which the rooms point into.
    std::string other_room_name { "A Room Elsewhere" };
    std::string other_room_description { "Another room.\n\r" };
    std::string load_room_name { LOAD_ROOM_NAME };
    std::string load_room_description { "A quiet room used for login tests.\n\r" };
};

// An empty news board for the life of the scope; entering the game counts its unread news.
class ScopedEmptyNewsBoard {
public:
    ScopedEmptyNewsBoard()
        : saved_board_count(board_info_type::num_of_boards)
        , saved_news_board(news_board)
    {
        board.lnum = 0;
        news_board = &board;
    }

    ~ScopedEmptyNewsBoard()
    {
        news_board = saved_news_board;
        board_info_type::num_of_boards = saved_board_count;
        std::free(board.msg_index);
    }

    ScopedEmptyNewsBoard(const ScopedEmptyNewsBoard&) = delete;
    ScopedEmptyNewsBoard& operator=(const ScopedEmptyNewsBoard&) = delete;

private:
    // The board count before the scope; constructing a board increments it.
    byte saved_board_count;
    // The news board before the scope.
    board_info_type* saved_news_board;
    // The empty board that stands in for the news board.
    board_info_type board;
};

// A connection driven through nanny() one line at a time. When destroyed, it frees the character it
// holds and any original body a switched immortal left with it.
class TestConnection {
public:
    explicit TestConnection(int state)
    {
        descriptor.descriptor = NO_SOCKET;
        descriptor.pos = -1;
        descriptor.connected = state;
        std::snprintf(descriptor.account_name, sizeof(descriptor.account_name), "%s", ACCOUNT_NAME);
        std::snprintf(descriptor.host, sizeof(descriptor.host), "%s", CONNECTION_HOST);
        clear_output();
    }

    ~TestConnection()
    {
        release_large_output_buffer();
        if (descriptor.pProtocol != nullptr) {
            ProtocolDestroy(descriptor.pProtocol);
        }
        free_owned_character(descriptor.character);
        free_owned_character(descriptor.original);
    }

    TestConnection(const TestConnection&) = delete;
    TestConnection& operator=(const TestConnection&) = delete;

    // Sends one line of player input, after discarding the output of the previous line.
    void send(std::string_view line)
    {
        clear_output();
        std::vector<char> input(line.begin(), line.end());
        input.push_back('\0');
        nanny(&descriptor, input.data());
    }

    // The text written to the connection since the last line was sent.
    std::string output() const
    {
        return descriptor.output;
    }

    // The connection's login state.
    int state() const
    {
        return descriptor.connected;
    }

    // The character the connection holds, or null.
    char_data* character() const
    {
        return descriptor.character;
    }

    // The underlying connection, for arranging a test.
    descriptor_data& data()
    {
        return descriptor;
    }

private:
    // Frees a character this connection still points at.
    void free_owned_character(char_data*& character)
    {
        if (character == nullptr) {
            return;
        }
        if (character->desc == &descriptor) {
            character->desc = nullptr;
        }
        free_char(character);
        character = nullptr;
    }

    // Returns the large output buffer, if write_to_output switched to one, to the shared pool.
    void release_large_output_buffer()
    {
        if (descriptor.large_outbuf != nullptr) {
            descriptor.large_outbuf->next = bufpool;
            bufpool = descriptor.large_outbuf;
            descriptor.large_outbuf = nullptr;
        }
    }

    // Empties the output and returns the connection to its small buffer.
    void clear_output()
    {
        release_large_output_buffer();
        descriptor.output = descriptor.small_outbuf;
        descriptor.small_outbuf[0] = '\0';
        descriptor.bufptr = 0;
        descriptor.bufspace = SMALL_BUFSIZE - 1;
    }

    // The connection nanny() reads and writes.
    descriptor_data descriptor { };
};

// A human character as stored on disk, saved in the load room, with stats the game accepts as sound.
char_file_u make_stored_character(const char* name, int level, long idnum)
{
    char_file_u stored_character { };
    std::snprintf(stored_character.name, sizeof(stored_character.name), "%s", name);
    stored_character.level = level;
    stored_character.race = RACE_HUMAN;
    stored_character.player_index = -1;
    stored_character.specials2.idnum = idnum;
    stored_character.specials2.load_room = LOAD_ROOM_VNUM;
    for (char_ability_data* abilities : { &stored_character.tmpabilities, &stored_character.constabilities }) {
        abilities->str = 15;
        abilities->lea = 15;
        abilities->intel = 15;
        abilities->wil = 15;
        abilities->dex = 15;
        abilities->con = 15;
        abilities->hit = 50;
        abilities->mana = 50;
        abilities->move = 50;
    }
    return stored_character;
}

// A copy of text in memory from malloc, which free_char releases names with.
char* copy_for_game(std::string_view text)
{
    char* copy = static_cast<char*>(std::malloc(text.size() + 1));
    if (copy == nullptr) {
        ADD_FAILURE() << "Could not allocate a copy of " << text;
        return nullptr;
    }
    std::memcpy(copy, text.data(), text.size());
    copy[text.size()] = '\0';
    return copy;
}

// A character already in the game, created as the login code creates one.
char_data* create_character_in_game(const char* name, int level, long idnum)
{
    char_data* character = nullptr;
    CREATE(character, struct char_data, 1);
    clear_char(character, MOB_VOID);
    character->player.name = copy_for_game(name);
    character->player.level = level;
    character->player.race = RACE_HUMAN;
    character->specials2.idnum = idnum;
    register_pc_char(character);
    return character;
}

// Puts character in the room at room_index, in the room's list of people and the game's list of
// characters.
void place_in_room(char_data* character, int room_index)
{
    character->in_room = room_index;
    character->next_in_room = world[room_index].people;
    world[room_index].people = character;
    character->next = character_list;
    character_list = character;
}

// Whether character is among the people in the room at room_index.
bool room_holds(int room_index, const char_data* character)
{
    for (const char_data* person = world[room_index].people; person != nullptr; person = person->next_in_room) {
        if (person == character) {
            return true;
        }
    }
    return false;
}

// Whether character is in the game's list of characters.
bool game_holds(const char_data* character)
{
    for (const char_data* listed = character_list; listed != nullptr; listed = listed->next) {
        if (listed == character) {
            return true;
        }
    }
    return false;
}

// The account file's copy of a linked character.
::testing::AssertionResult read_stored_character(const char* name, char_file_u* stored_character)
{
    std::string error_message;
    if (!account::read_account_character_file(".", ACCOUNT_NAME, name, stored_character, &error_message)) {
        return ::testing::AssertionFailure() << "Could not read " << name << ": " << error_message;
    }
    return ::testing::AssertionSuccess();
}

// Where text first appears in output, or npos.
std::size_t position_of(const std::string& output, std::string_view text)
{
    return output.find(text.data(), 0, text.size());
}

// Whether output ends with text.
bool ends_with(const std::string& output, std::string_view text)
{
    return output.size() >= text.size()
        && output.compare(output.size() - text.size(), text.size(), text.data(), text.size()) == 0;
}

// The account, the shared world state and one connection at the account roster, for driving the
// login from character selection into the game.
class LoginEntryTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::string error_message;
        ASSERT_TRUE(account::create_account(".", ACCOUNT_NAME, ACCOUNT_EMAIL, ACCOUNT_PASSWORD,
            ACCOUNT_CREATED_AT, nullptr, &error_message))
            << error_message;
    }

    void TearDown() override
    {
        // The characters a test leaves in the game are freed by their connections; unhook them
        // from the shared rooms and the character list first.
        world[0].people = nullptr;
        world[LOAD_ROOM_RNUM].people = nullptr;
        character_list = nullptr;
    }

    // Writes stored_character's files into the account and links it to the account, as the
    // account's existing character.
    ::testing::AssertionResult link_character(const char_file_u& stored_character)
    {
        std::string error_message;
        const std::string name = stored_character.name;
        if (!account::write_account_character_file(".", ACCOUNT_NAME, stored_character, &error_message)
            || !account::write_default_account_object_file(".", ACCOUNT_NAME, name, &error_message)
            || !account::write_default_account_exploit_file(".", ACCOUNT_NAME, name, &error_message)
            || !account::admin_link_character(".", ACCOUNT_NAME, name, CHARACTER_LINKED_AT, nullptr, &error_message)) {
            return ::testing::AssertionFailure() << "Could not link " << name << ": " << error_message;
        }
        return ::testing::AssertionSuccess();
    }

    // Stores preference_flags as the account's preferred configuration.
    ::testing::AssertionResult set_account_preferences(long preference_flags)
    {
        account::AccountData account_data;
        std::string error_message;
        if (!account::read_account_file(".", ACCOUNT_NAME, &account_data, &error_message)) {
            return ::testing::AssertionFailure() << "Could not read the account: " << error_message;
        }
        account_data.preferences.present = true;
        account_data.preferences.preference_flags = preference_flags;
        if (!account::write_account_file(".", account_data, &error_message)) {
            return ::testing::AssertionFailure() << "Could not write the account: " << error_message;
        }
        return ::testing::AssertionSuccess();
    }

    // Puts legacy_connection at the password prompt of a character with no account, gimli, whose
    // legacy player-table entry and password are in place and who has failed_logins recorded.
    void prepare_legacy_password_prompt(TestConnection& legacy_connection, int failed_logins, char_data** out_gimli)
    {
        std::error_code error;
        std::filesystem::create_directories("players/F-J", error);
        ASSERT_FALSE(error) << error.message();

        char_file_u stored_gimli = make_stored_character("gimli", 30, 7070);
        stored_gimli.specials2.bad_pws = failed_logins;
        const int player_index = create_entry("gimli");
        player_table[player_index].level = stored_gimli.level;
        player_table[player_index].race = stored_gimli.race;
        player_table[player_index].idnum = stored_gimli.specials2.idnum;

        descriptor_data& legacy_descriptor = legacy_connection.data();
        legacy_descriptor.account_name[0] = '\0';
        legacy_descriptor.pos = player_index;
        std::snprintf(legacy_descriptor.pwd, sizeof(legacy_descriptor.pwd), "%s", LEGACY_PASSWORD);
        char_data* gimli = nullptr;
        CREATE(gimli, struct char_data, 1);
        clear_char(gimli, MOB_VOID);
        register_pc_char(gimli);
        store_to_char(&stored_gimli, gimli);
        gimli->desc = &legacy_descriptor;
        legacy_descriptor.character = gimli;
        *out_gimli = gimli;
    }

    // Selects character_name at the roster and checks the character menu followed.
    void select_from_roster(TestConnection& selecting_connection, std::string_view character_name)
    {
        selecting_connection.send(character_name);
        ASSERT_EQ(selecting_connection.state(), CON_SLCT) << selecting_connection.output();
        ASSERT_NE(selecting_connection.character(), nullptr);
    }

    // The test's own directory, the working directory while it runs.
    TemporaryWorkingDirectory working_directory;
    // No connections in the game but the ones a test adds.
    ScopedGlobal<descriptor_data*> no_other_connections { descriptor_list, nullptr };
    // No characters in the game but the ones a test adds.
    ScopedGlobal<char_data*> no_characters_in_game { character_list, nullptr };
    // No player-table entries but the ones the login code or a test adds.
    ScopedEmptyPlayerTable empty_player_table;
    // The rooms characters enter.
    ScopedTestRooms test_rooms;
    // The news board entering the game reads.
    ScopedEmptyNewsBoard empty_news_board;
    // The mortals' message of the day, and the global the login code reads it from.
    std::string mortal_motd { MORTAL_MOTD };
    ScopedGlobal<char*> mortal_motd_in_use { motd, mortal_motd.data() };
    // The immortals' message of the day, and the global the login code reads it from.
    std::string immortal_motd { IMMORTAL_MOTD };
    ScopedGlobal<char*> immortal_motd_in_use { imotd, immortal_motd.data() };
    // The wizlock message, and the global the login code reads it from.
    std::string wizlock_message { WIZLOCK_MESSAGE };
    ScopedGlobal<char*> wizlock_message_in_use { wizlock_msg, wizlock_message.data() };
    // The level below which wizlock refuses logins; zero, no wizlock, unless a test sets one.
    ScopedGlobal<int> no_wizlock { restrict, 0 };
    // The banned sites; none unless a test adds one.
    ScopedGlobal<ban_list_element*> no_banned_sites { ban_list, nullptr };
    // The connection logging in, waiting at the account roster.
    TestConnection connection { CON_ACCTSLCT };
};

} // namespace

TEST_F(LoginEntryTest, SelectingACharacterShowsTheMotdAndTheCharacterMenu)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));

    connection.send("legolas");

    EXPECT_EQ(connection.state(), CON_SLCT);
    ASSERT_NE(connection.character(), nullptr);
    EXPECT_STREQ(GET_NAME(connection.character()), "legolas");
    EXPECT_EQ(connection.character()->desc, &connection.data());
    EXPECT_EQ(connection.output(), std::string(MORTAL_MOTD) + std::string(ACCOUNT_CHARACTER_MENU));
    ASSERT_EQ(top_of_p_table, 0);
    EXPECT_EQ(connection.data().pos, 0);
    EXPECT_STREQ(player_table[0].name, "legolas");
}

TEST_F(LoginEntryTest, SelectingACharacterRemovesItsLeftoverLegacyObjectAndExploitFiles)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    const std::filesystem::path legacy_object_file = account::legacy_object_file_path(".", "legolas");
    const std::filesystem::path legacy_exploits_file = account::legacy_exploits_file_path(".", "legolas");
    for (const std::filesystem::path& leftover : { legacy_object_file, legacy_exploits_file }) {
        std::error_code error;
        std::filesystem::create_directories(leftover.parent_path(), error);
        ASSERT_FALSE(error) << error.message();
        std::ofstream leftover_file(leftover, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(leftover_file.is_open()) << leftover;
        leftover_file << "left over";
    }

    connection.send("legolas");

    EXPECT_EQ(connection.state(), CON_SLCT);
    std::error_code error;
    EXPECT_FALSE(std::filesystem::exists(legacy_object_file, error)) << legacy_object_file;
    EXPECT_FALSE(std::filesystem::exists(legacy_exploits_file, error)) << legacy_exploits_file;
}

TEST_F(LoginEntryTest, SelectingAnImmortalShowsTheImmortalMotd)
{
    ASSERT_TRUE(link_character(make_stored_character("gandalf", LEVEL_IMMORT, 9191)));

    connection.send("gandalf");

    EXPECT_EQ(connection.state(), CON_SLCT);
    EXPECT_EQ(connection.output(), std::string(IMMORTAL_MOTD) + std::string(ACCOUNT_CHARACTER_MENU));
}

TEST_F(LoginEntryTest, SelectingACharacterShowsNoFailedLoginNoticeBecauseItsFileKeepsNoCount)
{
    // The account character file does not record failed password attempts, so a count set before
    // the file is written is gone when the character is selected.
    char_file_u legolas = make_stored_character("legolas", 45, 5252);
    legolas.specials2.bad_pws = 2;
    ASSERT_TRUE(link_character(legolas));

    connection.send("legolas");

    EXPECT_EQ(connection.state(), CON_SLCT);
    EXPECT_EQ(connection.output(), std::string(MORTAL_MOTD) + std::string(ACCOUNT_CHARACTER_MENU));
    ASSERT_NE(connection.character(), nullptr);
    EXPECT_EQ(connection.character()->specials2.bad_pws, 0);

    char_file_u saved { };
    ASSERT_TRUE(read_stored_character("legolas", &saved));
    EXPECT_EQ(saved.specials2.bad_pws, 0);
}

TEST_F(LoginEntryTest, SelectingADeletedCharacterRefusesAndStaysOnTheRoster)
{
    char_file_u legolas = make_stored_character("legolas", 45, 5252);
    SET_BIT(legolas.specials2.act, PLR_DELETED);
    ASSERT_TRUE(link_character(legolas));

    connection.send("legolas");

    EXPECT_EQ(connection.state(), CON_ACCTSLCT);
    EXPECT_EQ(connection.character(), nullptr);
    EXPECT_EQ(connection.data().pos, -1);
    const std::string output = connection.output();
    EXPECT_EQ(position_of(output, "That linked character is deleted and cannot be selected.\n\r"), 0u) << output;
    EXPECT_TRUE(ends_with(output, ROSTER_PROMPT_END)) << output;
}

TEST_F(LoginEntryTest, SelectingACharacterWithUnreadableObjectDataRefusesAndStaysOnTheRoster)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    {
        std::ofstream object_file(account::account_character_object_path(".", ACCOUNT_NAME, "legolas"),
            std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(object_file.is_open());
        object_file << "not object data";
    }

    connection.send("legolas");

    EXPECT_EQ(connection.state(), CON_ACCTSLCT);
    EXPECT_EQ(connection.character(), nullptr);
    const std::string output = connection.output();
    EXPECT_EQ(position_of(output,
                  "That linked character is missing required object-save data in account storage.\n\r"
                  "Expected JSON object.\n\r"),
        0u)
        << output;
    EXPECT_TRUE(ends_with(output, ROSTER_PROMPT_END)) << output;
}

TEST_F(LoginEntryTest, ACharacterNotClearedForABannedSiteIsRefusedAndDisconnected)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    ban_list_element site_ban { };
    std::snprintf(site_ban.site, sizeof(site_ban.site), "%s", CONNECTION_HOST);
    site_ban.type = BAN_SELECT;
    ban_list = &site_ban;

    connection.send("legolas");

    EXPECT_EQ(connection.state(), CON_CLOSE);
    EXPECT_EQ(connection.output(), "Sorry, this character has not been cleared for login from your site!\n\r");
}

TEST_F(LoginEntryTest, ACharacterClearedForABannedSiteIsAdmitted)
{
    char_file_u legolas = make_stored_character("legolas", 45, 5252);
    SET_BIT(legolas.specials2.act, PLR_SITEOK);
    ASSERT_TRUE(link_character(legolas));
    ban_list_element site_ban { };
    std::snprintf(site_ban.site, sizeof(site_ban.site), "%s", CONNECTION_HOST);
    site_ban.type = BAN_SELECT;
    ban_list = &site_ban;

    connection.send("legolas");

    EXPECT_EQ(connection.state(), CON_SLCT);
    EXPECT_EQ(connection.output(), std::string(MORTAL_MOTD) + std::string(ACCOUNT_CHARACTER_MENU));
}

TEST_F(LoginEntryTest, WizlockRefusesACharacterBelowTheRestrictionLevelAndDisconnects)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    restrict = 46;

    connection.send("legolas");

    EXPECT_EQ(connection.state(), CON_CLOSE);
    EXPECT_EQ(connection.output(),
        "The game is temporarily restricted.\r\nTry again later." + std::string(WIZLOCK_MESSAGE) + "\n\r");
}

TEST_F(LoginEntryTest, WizlockAdmitsACharacterAtTheRestrictionLevel)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    restrict = 45;

    connection.send("legolas");

    EXPECT_EQ(connection.state(), CON_SLCT);
    EXPECT_EQ(connection.output(), std::string(MORTAL_MOTD) + std::string(ACCOUNT_CHARACTER_MENU));
}

TEST_F(LoginEntryTest, SelectingASwitchedImmortalReconnectsToTheOriginalBody)
{
    ASSERT_TRUE(link_character(make_stored_character("gandalf", 95, 9595)));

    // Gandalf's player is switched into a mob; the original body waits in the load room.
    char_data* original_body = create_character_in_game("gandalf", 95, 9595);
    place_in_room(original_body, LOAD_ROOM_RNUM);
    char_data* mob = nullptr;
    CREATE(mob, struct char_data, 1);
    clear_char(mob, MOB_ISNPC);
    SET_BIT(mob->specials2.act, MOB_ISNPC);
    mob->nr = -1;
    mob->player.name = copy_for_game("mob");
    place_in_room(mob, LOAD_ROOM_RNUM);
    TestConnection switched_connection(CON_PLYNG);
    switched_connection.data().descriptor = 7;
    switched_connection.data().character = mob;
    switched_connection.data().original = original_body;
    mob->desc = &switched_connection.data();

    connection.send("gandalf");

    EXPECT_EQ(connection.state(), CON_PLYNG);
    EXPECT_EQ(connection.character(), original_body);
    EXPECT_EQ(original_body->desc, &connection.data());
    EXPECT_EQ(connection.output(), "Reconnecting to unswitched char.");
    EXPECT_EQ(switched_connection.state(), CON_CLOSE);
    EXPECT_EQ(switched_connection.character(), nullptr);
    EXPECT_EQ(switched_connection.data().original, nullptr);
    EXPECT_EQ(std::string(switched_connection.data().output), "Disconnecting.");

    // The game leaves the mob in the world; free it here once the old connection let it go.
    if (switched_connection.character() == nullptr) {
        mob->desc = nullptr;
        free_char(mob);
    }
}

TEST_F(LoginEntryTest, EnteringTheGamePutsTheCharacterInItsLoadRoomAndPlaying)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    select_from_roster(connection, "legolas");
    char_data* legolas = connection.character();

    connection.send("1");

    EXPECT_EQ(connection.state(), CON_PLYNG);
    ASSERT_EQ(connection.character(), legolas);
    EXPECT_EQ(legolas->in_room, LOAD_ROOM_RNUM);
    EXPECT_TRUE(room_holds(LOAD_ROOM_RNUM, legolas));
    EXPECT_TRUE(game_holds(legolas));
    EXPECT_EQ(connection.data().prompt_mode, 1);
    EXPECT_NE(connection.data().pProtocol, nullptr);
}

TEST_F(LoginEntryTest, EnteringTheGameAppliesTheAccountPreferences)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    ASSERT_TRUE(set_account_preferences(PRF_COMPACT | PRF_WRAP));
    select_from_roster(connection, "legolas");
    char_data* legolas = connection.character();
    ASSERT_FALSE(PRF_FLAGGED(legolas, PRF_COMPACT));
    ASSERT_FALSE(PRF_FLAGGED(legolas, PRF_WRAP));

    connection.send("1");

    EXPECT_TRUE(PRF_FLAGGED(legolas, PRF_COMPACT));
    EXPECT_TRUE(PRF_FLAGGED(legolas, PRF_WRAP));
}

TEST_F(LoginEntryTest, EnteringTheGameLoadsTheAliasesSavedWithTheCharactersObjects)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    objects_json::ObjectSaveData object_data;
    object_data.rent.rentcode = RENT_CRASH;
    object_data.aliases.push_back({ "gg", "get all corpse" });
    std::string object_bytes;
    std::string error_message;
    ASSERT_TRUE(objects_json::object_save_data_to_binary(object_data, &object_bytes, &error_message)) << error_message;
    ASSERT_TRUE(account::write_account_object_file(".", ACCOUNT_NAME, "legolas", object_bytes, &error_message))
        << error_message;
    select_from_roster(connection, "legolas");
    char_data* legolas = connection.character();
    ASSERT_EQ(legolas->specials.alias, nullptr);

    // Selection holds the object file's contents for the character; entering the game reads the
    // aliases from them.
    connection.send("1");

    const alias_list* alias = legolas->specials.alias;
    ASSERT_NE(alias, nullptr);
    EXPECT_STREQ(alias->keyword, "gg");
    EXPECT_STREQ(alias->command, "get all corpse");
    EXPECT_EQ(alias->next, nullptr);
}

TEST_F(LoginEntryTest, EnteringTheGameSendsTheArrivalTextAndTheRoomLook)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    select_from_roster(connection, "legolas");

    connection.send("1");

    // The news and mail reports, the welcome, then the room look: the room's name and its exits.
    // The character's preferences leave out the room description.
    EXPECT_EQ(connection.output(),
        "There is no unread news.\n\r"
        "There is no mail for you.\n\r"
        "\n\rHere we go...\n\r"
        "\n\r"
            + std::string(LOAD_ROOM_NAME) + "    Exits are:\n\r");
}

TEST_F(LoginEntryTest, EnteringTheGameSavesThePlayerFile)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    select_from_roster(connection, "legolas");
    char_file_u saved_at_selection { };
    ASSERT_TRUE(read_stored_character("legolas", &saved_at_selection));

    connection.send("1");

    // Selection saves the room number; entering saves again with the room's index in the world.
    char_file_u saved_at_entry { };
    ASSERT_TRUE(read_stored_character("legolas", &saved_at_entry));
    EXPECT_EQ(saved_at_selection.specials2.load_room, LOAD_ROOM_VNUM);
    EXPECT_EQ(saved_at_entry.specials2.load_room, LOAD_ROOM_RNUM);
}

TEST_F(LoginEntryTest, EnteringTheGameAfterAnotherConnectionUsedTheUnlockReturnsToTheAccountMenu)
{
    std::string error_message;
    ASSERT_TRUE(account::admin_link_character(".", ACCOUNT_NAME, "aragorn", CHARACTER_LINKED_AT, nullptr, &error_message))
        << error_message;
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    ASSERT_TRUE(link_character(make_stored_character("boromir", 40, 6262)));

    // Aragorn, below the level that lets an account play two characters, is already playing.
    TestConnection aragorn_connection(CON_PLYNG);
    aragorn_connection.data().descriptor = 7;
    char_data* aragorn = create_character_in_game("aragorn", 50, 4242);
    aragorn->desc = &aragorn_connection.data();
    aragorn_connection.data().character = aragorn;
    descriptor_list = &aragorn_connection.data();

    account::AccountData account_data;
    ASSERT_TRUE(account::read_account_file(".", ACCOUNT_NAME, &account_data, &error_message)) << error_message;
    ASSERT_TRUE(grant_account_character_selection_unlock(account_data, &error_message)) << error_message;

    // Two connections select under the one unlock; the first to enter the game uses it.
    TestConnection second_connection(CON_ACCTSLCT);
    select_from_roster(connection, "legolas");
    select_from_roster(second_connection, "boromir");
    connection.send("1");
    ASSERT_EQ(connection.state(), CON_PLYNG) << connection.output();

    second_connection.send("1");

    EXPECT_EQ(second_connection.state(), CON_ACCTMENU);
    EXPECT_EQ(second_connection.character(), nullptr);
    EXPECT_EQ(second_connection.data().pos, -1);
    EXPECT_EQ(second_connection.output(),
        "You are already connected as Aragorn. Reconnect to that character or log out before playing another character.\n\r"
        "\n\rAccount: player@example.com"
        "\n\rLinked characters: 3\n\r"
        "Active character: Aragorn (level 50, playing)\n\r"
        "\n\r"
        "1) List linked characters\n\r"
        "2) Play a linked character\n\r"
        "3) Add an existing character\n\r"
        "4) Create a new character\n\r"
        "5) Reset account password\n\r"
        "0) Log out\n\r"
        "\n\r"
        "Choice: ");

    // Aragorn's connection ends with this test.
    descriptor_list = nullptr;
}

TEST_F(LoginEntryTest, EnteringTheGameWhileTheCharacterPlaysElsewhereTakesOverThatBody)
{
    ASSERT_TRUE(link_character(make_stored_character("legolas", 45, 5252)));
    select_from_roster(connection, "legolas");
    char_data* selected_copy = connection.character();

    // Between the selection and entering, the same character comes into play on another connection.
    TestConnection other_connection(CON_PLYNG);
    other_connection.data().descriptor = 7;
    char_data* playing_body = create_character_in_game("legolas", 45, 5252);
    playing_body->desc = &other_connection.data();
    other_connection.data().character = playing_body;
    place_in_room(playing_body, LOAD_ROOM_RNUM);
    ASSERT_NE(selected_copy, playing_body);

    connection.send("1");

    EXPECT_EQ(connection.state(), CON_PLYNG);
    EXPECT_EQ(connection.character(), playing_body);
    EXPECT_EQ(playing_body->desc, &connection.data());
    EXPECT_EQ(connection.output(), "You take over your own body, already in use!\n\r");
    EXPECT_EQ(other_connection.state(), CON_CLOSE);
    EXPECT_EQ(other_connection.character(), nullptr);
    EXPECT_EQ(std::string(other_connection.data().output), "This body has been usurped!\n\r");
}

TEST_F(LoginEntryTest, LegacyPasswordLoginShowsTheMotdAndTheLegacyMenu)
{
    TestConnection legacy_connection(CON_PWDNRM);
    char_data* gimli = nullptr;
    ASSERT_NO_FATAL_FAILURE(prepare_legacy_password_prompt(legacy_connection, 0, &gimli));

    legacy_connection.send(LEGACY_PASSWORD);

    EXPECT_EQ(legacy_connection.state(), CON_SLCT);
    EXPECT_EQ(legacy_connection.character(), gimli);
    EXPECT_EQ(legacy_connection.output(), std::string(MORTAL_MOTD) + std::string(LEGACY_CHARACTER_MENU));
}

TEST_F(LoginEntryTest, LegacyPasswordLoginReportsFailedLoginsAndSavesThemCleared)
{
    TestConnection legacy_connection(CON_PWDNRM);
    char_data* gimli = nullptr;
    ASSERT_NO_FATAL_FAILURE(prepare_legacy_password_prompt(legacy_connection, 2, &gimli));

    legacy_connection.send(LEGACY_PASSWORD);

    EXPECT_EQ(legacy_connection.state(), CON_SLCT);
    EXPECT_EQ(legacy_connection.output(),
        std::string(MORTAL_MOTD) + "\n\r\n\r2 LOGIN FAILURES SINCE LAST SUCCESSFUL LOGIN.\n\r"
            + std::string(LEGACY_CHARACTER_MENU));
    EXPECT_EQ(gimli->specials2.bad_pws, 0);

    char name[] = "gimli";
    char_file_u saved { };
    ASSERT_GE(load_char(name, &saved), 0);
    EXPECT_EQ(saved.specials2.bad_pws, 0);
}

TEST_F(LoginEntryTest, LegacyPasswordLoginReportsOneFailedLoginInTheSingular)
{
    TestConnection legacy_connection(CON_PWDNRM);
    char_data* gimli = nullptr;
    ASSERT_NO_FATAL_FAILURE(prepare_legacy_password_prompt(legacy_connection, 1, &gimli));

    legacy_connection.send(LEGACY_PASSWORD);

    EXPECT_EQ(legacy_connection.state(), CON_SLCT);
    EXPECT_EQ(legacy_connection.output(),
        std::string(MORTAL_MOTD) + "\n\r\n\r1 LOGIN FAILURE SINCE LAST SUCCESSFUL LOGIN.\n\r"
            + std::string(LEGACY_CHARACTER_MENU));
}
