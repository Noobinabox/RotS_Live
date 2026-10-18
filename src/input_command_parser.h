#ifndef INPUT_COMMAND_PARSER_H
#define INPUT_COMMAND_PARSER_H

#include <cstddef>
#include <string_view>

// Reading a typed command line. These functions see only text and the tables they are given;
// whether a character may use what they find is the caller's decision. None allocates on valid
// input (only logging a null argument does), and every returned view points into the text passed
// in.

// What a typed line starts with, after its leading spaces.
enum class typed_line_kind : unsigned char {
    // Nothing after the leading spaces.
    blank,
    // A ' that stands for say.
    say_shortcut,
    // A ; that stands for wiznet.
    wiznet_shortcut,
    // A , that stands for emote.
    emote_shortcut,
    // Anything else, including a line that starts with /.
    word,
};

// A typed line taken apart. Every view points into the line given to parse_typed_line.
// The views come first and the one-byte members last, so the struct carries no padding between
// members.
struct typed_line {
    // The line after its leading spaces; the other views lie inside it.
    std::string_view text;
    // For a word line, the command word as typed: from the first character up to the first
    // character that is <= ' ' as an unsigned char. Empty when the line starts with such a
    // character, and for blank and shortcut lines.
    std::string_view command_word;
    // For a word line, everything after command_word, the character that ended it included.
    // For a shortcut line, everything after the shortcut character. Empty for a blank line.
    std::string_view handler_text;
    // The text after the '/' when starts_with_shape_marker is true; empty otherwise.
    std::string_view shape_text;
    // What the line starts with.
    typed_line_kind kind = typed_line_kind::blank;
    // True when text starts with '/', the shape shortcut. Such a line is still a word line.
    bool starts_with_shape_marker = false;
};

// Takes a typed line apart. Only ' ' counts as a leading space.
[[nodiscard]] typed_line parse_typed_line(std::string_view line);

// find_command_number's answer for an empty command word, as the game's older table search gives.
inline constexpr int empty_command_word_number = 0;
// find_command_number's answer when no command matches.
inline constexpr int unknown_command_number = -1;

// The number of the first command, in table order, that command_word is a prefix of, comparing
// command_word with only A-Z lowercased. Numbers count from 1 for the table's first entry.
// Returns empty_command_word_number for an empty command_word without searching, and
// unknown_command_number when nothing matches.
// command_names is a list of lowercase names ended by an entry that starts with '\n'. A null
// command_names is logged and returns unknown_command_number.
[[nodiscard]] int find_command_number(std::string_view command_word,
    const char* const* command_names);

// The word a social is looked up by, read from the start of text the way the game reads a
// command's first argument: whitespace (' ', \t, \n, \v, \f, \r) is skipped; a word runs up to
// the first character that is <= ' ' as an unsigned char; and the fill words in, from, with, the,
// on, at and to, matched with A-Z lowercased, are skipped. The view is as typed; the caller
// lowercases it. Empty when no word follows.
// A word that starts with ' gives the first letter of the last fill word skipped before it, or an
// empty view when none was; see the definition for why.
[[nodiscard]] std::string_view read_social_word(std::string_view text);

// The text up to its terminating '\0', or its first max_length characters when no terminator
// comes sooner; reads no further than that. A null text is logged and gives an empty view.
[[nodiscard]] std::string_view view_terminated_text(const char* text, std::size_t max_length);

#endif // INPUT_COMMAND_PARSER_H
