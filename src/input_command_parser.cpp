#include "command_line_parser.h"

#include "utils.h"

#include <algorithm>
#include <array>
#include <string>

namespace {

// Compared as unsigned so that bytes 0x80 and above stay inside a word, whatever the signedness
// of char.
bool ends_word(char character)
{
    return static_cast<unsigned char>(character) <= ' ';
}

char lowercase_letter(char character)
{
    if (character >= 'A' && character <= 'Z') {
        return static_cast<char>(character - 'A' + 'a');
    }
    return character;
}

// The characters isspace() accepts in the C locale, which the game never changes.
constexpr std::string_view social_whitespace = " \t\n\v\f\r";

// Words skipped before a social's word, as in "smile at bob". The same words as the game's fill
// table in interpre.cpp; a test fails when the two differ.
constexpr std::array<std::string_view, 7> fill_words
    = { "in", "from", "with", "the", "on", "at", "to" };

bool equals_lowercased(std::string_view word, std::string_view lowercase_word)
{
    if (word.size() != lowercase_word.size()) {
        return false;
    }
    return std::equal(word.begin(), word.end(), lowercase_word.begin(),
        [](char word_character, char lowercase_character) -> bool {
            return lowercase_letter(word_character) == lowercase_character;
        });
}

bool is_fill_word(std::string_view word)
{
    return std::any_of(fill_words.begin(), fill_words.end(),
        [word](std::string_view fill_word) -> bool { return equals_lowercased(word, fill_word); });
}

bool starts_with_lowercased(std::string_view name, std::string_view word)
{
    if (name.size() < word.size()) {
        return false;
    }
    return std::equal(word.begin(), word.end(), name.begin(),
        [](char word_character, char name_character) -> bool {
            return lowercase_letter(word_character) == name_character;
        });
}

} // namespace

typed_line parse_typed_line(std::string_view line)
{
    typed_line parsed;

    const std::size_t first_non_space = line.find_first_not_of(' ');
    if (first_non_space == std::string_view::npos) {
        parsed.text = line.substr(line.size());
        return parsed;
    }
    parsed.text = line.substr(first_non_space);

    const char first_character = parsed.text.front();
    if (first_character == '\'') {
        parsed.kind = typed_line_kind::say_shortcut;
        parsed.handler_text = parsed.text.substr(1);
        return parsed;
    }
    if (first_character == ';') {
        parsed.kind = typed_line_kind::wiznet_shortcut;
        parsed.handler_text = parsed.text.substr(1);
        return parsed;
    }
    if (first_character == ',') {
        parsed.kind = typed_line_kind::emote_shortcut;
        parsed.handler_text = parsed.text.substr(1);
        return parsed;
    }

    parsed.kind = typed_line_kind::word;
    if (first_character == '/') {
        parsed.starts_with_shape_marker = true;
        parsed.shape_text = parsed.text.substr(1);
    }

    const auto word_end = std::find_if(parsed.text.begin(), parsed.text.end(), ends_word);
    const std::size_t word_length = static_cast<std::size_t>(word_end - parsed.text.begin());
    parsed.command_word = parsed.text.substr(0, word_length);
    parsed.handler_text = parsed.text.substr(word_length);
    return parsed;
}

int find_command_number(std::string_view command_word, const char* const* command_names)
{
    if (command_names == nullptr) {
        log((std::string("SYSERR: ") + __func__ + ": called with a null command table").c_str());
        return -1;
    }
    if (command_word.empty()) {
        return 0;
    }

    for (int index = 0; command_names[index][0] != '\n'; ++index) {
        if (starts_with_lowercased(command_names[index], command_word)) {
            return index + 1;
        }
    }
    return -1;
}

std::string_view view_terminated_text(const char* text, std::size_t max_length)
{
    if (text == nullptr) {
        log((std::string("SYSERR: ") + __func__ + ": called with null text").c_str());
        return {};
    }

    const char* terminator = std::char_traits<char>::find(text, max_length, '\0');
    if (terminator == nullptr) {
        return std::string_view(text, max_length);
    }
    return std::string_view(text, static_cast<std::size_t>(terminator - text));
}

std::string_view read_social_word(std::string_view text)
{
    std::string_view remaining = text;
    std::string_view last_fill_word;

    for (;;) {
        const std::size_t word_start = remaining.find_first_not_of(social_whitespace);
        if (word_start == std::string_view::npos) {
            return remaining.substr(remaining.size());
        }
        remaining.remove_prefix(word_start);

        // A word that starts with a quote is not read. The result is the first letter of the last
        // fill word skipped, or nothing when none was, so "the 'bob" performs the "t" social at
        // bob. Players rely on what a typed line does, so this matches the game's first-argument
        // reading rather than reading the quoted word.
        if (remaining.front() == '\'') {
            return last_fill_word.substr(0, 1);
        }

        const auto word_end = std::find_if(remaining.begin(), remaining.end(), ends_word);
        const std::size_t word_length = static_cast<std::size_t>(word_end - remaining.begin());
        const std::string_view word = remaining.substr(0, word_length);
        if (!is_fill_word(word)) {
            return word;
        }
        last_fill_word = word;
        remaining.remove_prefix(word_length);
    }
}
