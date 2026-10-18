#include "command_line_parser.h"

#include "utils.h"

#include <algorithm>
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
