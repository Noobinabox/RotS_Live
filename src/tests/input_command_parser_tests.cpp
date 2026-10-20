#include "../input_command_parser.h"

#include <gtest/gtest.h>

#include <string_view>

namespace {

// A small command table in the game's format: lowercase names, an empty name like the game's
// bounce entry, and a "\n" entry that ends it.
const char* const TEST_COMMAND_NAMES[] = { "north", "east", "south", "say", "", "\n" };

bool lies_inside(std::string_view part, std::string_view whole)
{
    return part.data() >= whole.data() && part.data() + part.size() <= whole.data() + whole.size();
}

} // namespace

TEST(ParseTypedLine, SkipsOnlySpaces)
{
    const typed_line spaced = parse_typed_line("  look");
    EXPECT_EQ(spaced.text, "look");

    const typed_line tabbed = parse_typed_line("\tlook");
    EXPECT_EQ(tabbed.text, "\tlook");
    EXPECT_EQ(tabbed.kind, typed_line_kind::word);
    EXPECT_EQ(tabbed.command_word, "");
    EXPECT_EQ(tabbed.handler_text, "\tlook");
}

TEST(ParseTypedLine, FindsABlankLine)
{
    EXPECT_EQ(parse_typed_line("").kind, typed_line_kind::blank);
    EXPECT_EQ(parse_typed_line("   ").kind, typed_line_kind::blank);
}

TEST(ParseTypedLine, SplitsTheCommandWordFromTheHandlerText)
{
    const typed_line with_text = parse_typed_line("look  at sky");
    EXPECT_EQ(with_text.kind, typed_line_kind::word);
    EXPECT_EQ(with_text.command_word, "look");
    EXPECT_EQ(with_text.handler_text, "  at sky");

    const typed_line word_only = parse_typed_line("look");
    EXPECT_EQ(word_only.command_word, "look");
    EXPECT_EQ(word_only.handler_text, "");
}

TEST(ParseTypedLine, EndsTheWordAtAControlCharacterButNotAtAHighByte)
{
    EXPECT_EQ(parse_typed_line("lo\x01ok").command_word, "lo");
    EXPECT_EQ(parse_typed_line("lo\xE9k x").command_word, "lo\xE9k");
}

TEST(ParseTypedLine, RecognisesTheThreeShortcuts)
{
    const typed_line say = parse_typed_line("'hi");
    EXPECT_EQ(say.kind, typed_line_kind::say_shortcut);
    EXPECT_EQ(say.handler_text, "hi");

    const typed_line wiznet = parse_typed_line(";hi");
    EXPECT_EQ(wiznet.kind, typed_line_kind::wiznet_shortcut);
    EXPECT_EQ(wiznet.handler_text, "hi");

    const typed_line emote = parse_typed_line(",hi");
    EXPECT_EQ(emote.kind, typed_line_kind::emote_shortcut);
    EXPECT_EQ(emote.handler_text, "hi");

    const typed_line lone = parse_typed_line("'");
    EXPECT_EQ(lone.kind, typed_line_kind::say_shortcut);
    EXPECT_EQ(lone.handler_text, "");
}

TEST(ParseTypedLine, MarksTheShapeShortcutAndKeepsTheWord)
{
    const typed_line shape = parse_typed_line(" /mob 1");

    EXPECT_TRUE(shape.starts_with_shape_marker);
    EXPECT_EQ(shape.shape_text, "mob 1");
    EXPECT_EQ(shape.kind, typed_line_kind::word);
    EXPECT_EQ(shape.command_word, "/mob");
    EXPECT_EQ(shape.handler_text, " 1");

    EXPECT_FALSE(parse_typed_line("look /x").starts_with_shape_marker);
}

TEST(ParseTypedLine, PointsIntoTheGivenLine)
{
    const std::string_view line = "  look at sky";

    const typed_line parsed = parse_typed_line(line);

    EXPECT_TRUE(lies_inside(parsed.text, line));
    EXPECT_TRUE(lies_inside(parsed.command_word, line));
    EXPECT_TRUE(lies_inside(parsed.handler_text, line));
}

TEST(FindCommandNumber, MatchesThePrefixOfTheFirstEntryInTableOrder)
{
    EXPECT_EQ(find_command_number("s", TEST_COMMAND_NAMES), 3);
    EXPECT_EQ(find_command_number("sa", TEST_COMMAND_NAMES), 4);
    EXPECT_EQ(find_command_number("north", TEST_COMMAND_NAMES), 1);
    EXPECT_EQ(find_command_number("NoRTH", TEST_COMMAND_NAMES), 1);
}

TEST(FindCommandNumber, ReturnsZeroForAnEmptyWord)
{
    EXPECT_EQ(find_command_number("", TEST_COMMAND_NAMES), EMPTY_COMMAND_WORD_NUMBER);
}

TEST(FindCommandNumber, ReturnsMinusOneWhenNothingMatches)
{
    EXPECT_EQ(find_command_number("northx", TEST_COMMAND_NAMES), UNKNOWN_COMMAND_NUMBER);
    EXPECT_EQ(find_command_number("x", TEST_COMMAND_NAMES), UNKNOWN_COMMAND_NUMBER);
}

TEST(FindCommandNumber, NeverMatchesAnEmptyName)
{
    EXPECT_EQ(find_command_number("q", TEST_COMMAND_NAMES), UNKNOWN_COMMAND_NUMBER);
}

TEST(FindCommandNumber, StopsAtTheLastEntryOfATableWithNoEndEntry)
{
    const char* const names_without_end_entry[] = { "north", "east" };

    EXPECT_EQ(find_command_number("e", names_without_end_entry), 2);
    EXPECT_EQ(find_command_number("x", names_without_end_entry), UNKNOWN_COMMAND_NUMBER);
}

TEST(ViewTerminatedText, StopsAtTheTerminatorOrTheLimit)
{
    EXPECT_EQ(view_terminated_text("abc", 10), "abc");

    const char unterminated[5] = { 'a', 'b', 'c', 'd', 'e' };
    EXPECT_EQ(view_terminated_text(unterminated, sizeof(unterminated)), "abcde");

    EXPECT_EQ(view_terminated_text("abcdef", 3), "abc");
}

TEST(ViewTerminatedText, GivesAnEmptyViewForNullText)
{
    EXPECT_TRUE(view_terminated_text(nullptr, 10).empty());
}

TEST(ReadSocialWord, SkipsEveryKindOfWhitespace)
{
    EXPECT_EQ(read_social_word(" \t\n\v\f\rsmile x"), "smile");
}

TEST(ReadSocialWord, SkipsFillWordsInAnyCase)
{
    EXPECT_EQ(read_social_word("the at smile"), "smile");
    EXPECT_EQ(read_social_word("THE Smile"), "Smile");
}

TEST(ReadSocialWord, IsEmptyWhenOnlyFillWordsRemain)
{
    EXPECT_TRUE(read_social_word("the").empty());
    EXPECT_TRUE(read_social_word("from  ").empty());
    EXPECT_TRUE(read_social_word("").empty());
}

TEST(ReadSocialWord, KeepsAHighByteInsideTheWord)
{
    EXPECT_EQ(read_social_word("sm\xE9le"), "sm\xE9le");
}

TEST(ReadSocialWord, ReadsAQuotedWordAsTheLastFillWordsFirstLetter)
{
    EXPECT_EQ(read_social_word("the 'bob"), "t");
    EXPECT_EQ(read_social_word("from 'x"), "f");
    EXPECT_EQ(read_social_word("the at 'x"), "a");
    EXPECT_EQ(read_social_word("THE 'x"), "T");

    const std::string_view line = "the 'bob";
    EXPECT_EQ(read_social_word(line).data(), line.data()) << "points at the fill word's letter";
}

TEST(ReadSocialWord, ReadsABareQuotedWordAsEmpty)
{
    EXPECT_TRUE(read_social_word("'x").empty());
    EXPECT_TRUE(read_social_word(" 'x").empty());
}
