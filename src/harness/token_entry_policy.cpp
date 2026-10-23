/* token_entry_policy.cpp */

#include "token_entry_policy.h"

#include "account_management.h"
#include "comm.h"
#include "login_steps.h"
#include "structs.h"
#include "utils.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace harness {

namespace {
// The account storage root the login dialogue reads, relative to the game's data directory.
const std::string ACCOUNT_STORAGE_ROOT = ".";
// Stands in for the character's name in the log when a line is too malformed to name one.
constexpr std::string_view UNNAMED_CHARACTER = "(no character named)";
// Line endings, which a reason sent or logged on one line may not hold.
constexpr std::string_view LINE_ENDINGS = "\r\n";

// The words of an entry line after ENTRY_WORD.
struct EntryLine {
    // The token the line carries.
    std::string_view token;
    // The email of the account the character is linked to.
    std::string_view account_email;
    // The character to enter as.
    std::string_view character_name;
};

// Removes the next space-separated word from the front of remaining, with the spaces before it,
// and returns it; returns an empty word when only spaces are left.
std::string_view take_word(std::string_view& remaining)
{
    const std::size_t word_start = remaining.find_first_not_of(' ');
    if (word_start == std::string_view::npos) {
        remaining = std::string_view();
        return std::string_view();
    }
    remaining.remove_prefix(word_start);
    const std::string_view word = remaining.substr(0, remaining.find(' '));
    remaining.remove_prefix(word.size());
    return word;
}

// Reads the three words that follow ENTRY_WORD. Returns nothing unless there are exactly three.
std::optional<EntryLine> parse_entry_words(std::string_view words)
{
    EntryLine entry;
    entry.token = take_word(words);
    entry.account_email = take_word(words);
    entry.character_name = take_word(words);
    const std::string_view extra_word = take_word(words);
    if (entry.character_name.empty() || !extra_word.empty()) {
        return std::nullopt;
    }
    return entry;
}

// Returns true when offered equals expected. Every character of expected is compared whatever
// the first difference, so the time a refusal takes does not reveal how much of a token was right.
bool tokens_match(std::string_view offered, std::string_view expected)
{
    unsigned int difference = 0;
    if (offered.size() != expected.size()) {
        difference = 1;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        char offered_character = '\0';
        if (index < offered.size()) {
            offered_character = offered[index];
        }
        difference |= static_cast<unsigned char>(offered_character) ^ static_cast<unsigned char>(expected[index]);
    }
    return difference == 0;
}

// Returns text on one line: each run of line endings inside it becomes one space, and those at
// either end are dropped.
std::string one_line(std::string_view text)
{
    std::string line;
    line.reserve(text.size());
    std::string_view remaining = text;
    while (!remaining.empty()) {
        const std::size_t ending_start = remaining.find_first_of(LINE_ENDINGS);
        const std::string_view run = remaining.substr(0, ending_start);
        line.append(run);
        remaining.remove_prefix(run.size());
        const std::size_t next_text = remaining.find_first_not_of(LINE_ENDINGS);
        if (next_text == std::string_view::npos) {
            break;
        }
        if (!line.empty()) {
            line += ' ';
        }
        remaining.remove_prefix(next_text);
    }
    return line;
}

// Returns reason, or fallback when reason is empty.
std::string_view reason_or(std::string_view reason, std::string_view fallback)
{
    if (reason.empty()) {
        return fallback;
    }
    return reason;
}

// Refuses the entry: tells the connection reason, logs it with character_name, and closes the
// connection.
void refuse_entry(descriptor_data& connection, std::string_view character_name, std::string_view reason)
{
    const std::string reason_line = one_line(reason);

    std::string message(TokenEntryPolicy::REFUSAL_PREFIX);
    message.append(reason_line);
    message += "\n\r";
    SEND_TO_Q(message.c_str(), &connection);

    std::string log_line = "SYSERR: harness::TokenEntryPolicy::try_enter: ";
    log_line.append(character_name);
    log_line += ": ";
    log_line.append(reason_line);
    log(log_line.c_str());

    STATE(&connection) = CON_CLOSE;
}
} // namespace

//============================================================================
TokenEntryPolicy::TokenEntryPolicy(std::string run_token)
    : token(std::move(run_token))
{
}

//============================================================================
bool TokenEntryPolicy::try_enter(descriptor_data& connection, std::string_view line)
{
    std::string_view words = line;
    if (take_word(words) != ENTRY_WORD) {
        return false;
    }

    const std::optional<EntryLine> entry = parse_entry_words(words);
    if (!entry.has_value()) {
        refuse_entry(connection, UNNAMED_CHARACTER,
            "an entry line is \"harness-enter <token> <account email> <character>\".");
        return true;
    }
    // An empty token never matches: an entry line's token is a word, never empty.
    if (!tokens_match(entry->token, token)) {
        refuse_entry(connection, entry->character_name, "the token is not this run's.");
        return true;
    }

    account::AccountData account_data;
    std::string error_message;
    const std::string email(entry->account_email);
    if (!account::read_account_file_by_email(ACCOUNT_STORAGE_ROOT, email, &account_data, &error_message)) {
        refuse_entry(connection, entry->character_name, reason_or(error_message, "the account could not be read."));
        return true;
    }
    bind_connection_to_account(connection, account_data);

    // The roster's own match, so a name resolves exactly as typing it at the roster would.
    std::string selected_name;
    const std::string typed_name(entry->character_name);
    if (!account::select_linked_character(ACCOUNT_STORAGE_ROOT, account_data, typed_name,
            account::RosterSort::Name, account::RosterFilter::None, &selected_name, &error_message)) {
        refuse_entry(connection, entry->character_name,
            reason_or(error_message, "the character is not linked to the account."));
        return true;
    }

    const AccountCharacterSelection selection = select_account_character(connection, account_data, selected_name);
    if (!selection.selected) {
        refuse_entry(connection, entry->character_name,
            reason_or(selection.refusal, "the character could not be selected."));
        return true;
    }

    const Admission admission = admit_selected_character(connection);
    if (admission != Admission::Admitted) {
        // Refused has already told the connection and logged why; Reconnected is playing.
        return true;
    }

    std::string refusal;
    if (!claim_account_session(connection, account_data, refusal)) {
        refuse_entry(connection, entry->character_name,
            reason_or(refusal, "another session on the account forbids the character."));
        return true;
    }

    const std::string entered_message = "Harness entry: " + selected_name;
    log(entered_message.c_str());
    enter_game(connection);
    return true;
}

} // namespace harness
