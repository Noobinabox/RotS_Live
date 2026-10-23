/* login_test_fixtures.h */

#ifndef LOGIN_TEST_FIXTURES_H
#define LOGIN_TEST_FIXTURES_H

#include "account_management.h"
#include "boards.h"
#include "db.h"
#include "handler.h"
#include "interpre.h"
#include "protocol.h"
#include "structs.h"
#include "utils.h"
#include "zone.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
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

// Portable fixtures for tests that take a connection from an account's characters into the game:
// an account in a temporary directory, a two-room world, and a connection driven a line at a time.
namespace login_tests {

// The account every test logs in to.
inline constexpr char ACCOUNT_NAME[] = "acct";
// That account's email address, as the account menu shows it.
inline constexpr char ACCOUNT_EMAIL[] = "player@example.com";
// That account's password.
inline constexpr char ACCOUNT_PASSWORD[] = "ValidPass1";
// When the account was created; any fixed time will do.
inline constexpr long ACCOUNT_CREATED_AT = 1700010200;
// When characters are linked to the account.
inline constexpr long CHARACTER_LINKED_AT = 1700010201;
// The address the connection logging in comes from.
inline constexpr char CONNECTION_HOST[] = "127.0.0.1";
// A socket number no write reaches. The login code sends telnet negotiation straight to the
// socket, and it skips the room look for a connection whose socket number is zero.
inline constexpr int NO_SOCKET = -1;

// The room test characters are saved in, and the index the test world gives it.
inline constexpr int LOAD_ROOM_VNUM = 3001;
inline constexpr int LOAD_ROOM_RNUM = 1;
// A second room ahead of it in the world, so the load room's index differs from zero.
inline constexpr int OTHER_ROOM_VNUM = 1000;
// The load room's name, which the room look on entering the game shows.
inline constexpr char LOAD_ROOM_NAME[] = "The Testing Meadow";

// The message of the day each kind of character is shown.
inline constexpr std::string_view MORTAL_MOTD = "Mortal message of the day.\n\r";
inline constexpr std::string_view IMMORTAL_MOTD = "Immortal message of the day.\n\r";
// The wizlock message an immortal set.
inline constexpr std::string_view WIZLOCK_MESSAGE = "Closed for testing.\n\r";

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

    // Empties the output and returns the connection to its small buffer, as sending a line does.
    void clear_output()
    {
        release_large_output_buffer();
        descriptor.output = descriptor.small_outbuf;
        descriptor.small_outbuf[0] = '\0';
        descriptor.bufptr = 0;
        descriptor.bufspace = SMALL_BUFSIZE - 1;
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

    // The connection nanny() reads and writes.
    descriptor_data descriptor { };
};

// A human character as stored on disk, saved in the load room, with stats the game accepts as sound.
inline char_file_u make_stored_character(const char* name, int level, long idnum)
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
inline char* copy_for_game(std::string_view text)
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
inline char_data* create_character_in_game(const char* name, int level, long idnum)
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
inline void place_in_room(char_data* character, int room_index)
{
    character->in_room = room_index;
    character->next_in_room = world[room_index].people;
    world[room_index].people = character;
    character->next = character_list;
    character_list = character;
}

// Whether character is among the people in the room at room_index.
inline bool room_holds(int room_index, const char_data* character)
{
    for (const char_data* person = world[room_index].people; person != nullptr; person = person->next_in_room) {
        if (person == character) {
            return true;
        }
    }
    return false;
}

// Whether character is in the game's list of characters.
inline bool game_holds(const char_data* character)
{
    for (const char_data* listed = character_list; listed != nullptr; listed = listed->next) {
        if (listed == character) {
            return true;
        }
    }
    return false;
}

// The account file's copy of a linked character.
inline ::testing::AssertionResult read_stored_character(const char* name, char_file_u* stored_character)
{
    std::string error_message;
    if (!account::read_account_character_file(".", ACCOUNT_NAME, name, stored_character, &error_message)) {
        return ::testing::AssertionFailure() << "Could not read " << name << ": " << error_message;
    }
    return ::testing::AssertionSuccess();
}

// The account, the shared world state and an empty game, for taking a connection from the
// account's characters into the game. Each test runs in its own temporary working directory.
class LoginWorldTest : public ::testing::Test {
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
};

} // namespace login_tests

#endif /* LOGIN_TEST_FIXTURES_H */
