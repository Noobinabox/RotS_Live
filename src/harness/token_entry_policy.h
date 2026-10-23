/* token_entry_policy.h */

#ifndef HARNESS_TOKEN_ENTRY_POLICY_H
#define HARNESS_TOKEN_ENTRY_POLICY_H

#include "connection_entry_policy.h"

#include <string>
#include <string_view>

namespace harness {

// Takes a scenario's character straight into the game when a connection's first line is an entry
// line carrying this run's token: "harness-enter <token> <account email> <character>".
class TokenEntryPolicy : public ConnectionEntryPolicy {
public:
    // The first word of an entry line. No email address can be this word.
    static constexpr std::string_view ENTRY_WORD = "harness-enter";
    // What a connection is told before the reason its entry was refused.
    static constexpr std::string_view REFUSAL_PREFIX = "Harness entry refused: ";

    // run_token is the secret an entry line must carry. An empty run_token matches no line.
    explicit TokenEntryPolicy(std::string run_token);

    // Returns false, leaving the connection untouched, for any line whose first word is not
    // ENTRY_WORD. Otherwise handles the line and returns true: enters the named character through
    // the login steps, or sends the connection REFUSAL_PREFIX and the reason, logs the refusal
    // with the character's name and closes the connection. When the game itself refuses the
    // character at admission (site ban or wizlock), its own words come first. Words are separated
    // by spaces; a line with other than four words is refused, as is an account the password
    // login would refuse whatever the password (account::check_account_may_log_in) and a
    // character given by roster number instead of by name.
    [[nodiscard]] bool try_enter(descriptor_data& connection, std::string_view line) override;

private:
    // The secret every entry line must carry: this run's token (see create_run_token).
    std::string token;
};

} // namespace harness

#endif /* HARNESS_TOKEN_ENTRY_POLICY_H */
