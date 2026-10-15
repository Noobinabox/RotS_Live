#include "background_line_reader.h"

#include <chrono>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
// How long a test waits for the reader thread to reach the end of a string stream.
constexpr std::chrono::seconds READER_WAIT_LIMIT(5);
// A line far longer than any request, to show the reader does not cut lines.
constexpr std::size_t LONG_LINE_LENGTH = 200;

// Waits until reader reports its input closed; returns false after READER_WAIT_LIMIT.
bool wait_until_input_closed(const harness::BackgroundLineReader& reader)
{
    const std::chrono::steady_clock::time_point deadline
        = std::chrono::steady_clock::now() + READER_WAIT_LIMIT;
    while (!reader.input_closed()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

// Reads all of input_text through a reader and returns the lines it received. The stream is
// declared before the reader, so it outlives the reader even when the wait times out.
std::vector<std::string> lines_read_from(const std::string& input_text)
{
    std::istringstream input(input_text);
    harness::BackgroundLineReader reader(input);
    EXPECT_TRUE(wait_until_input_closed(reader)) << "Expected the reader to reach end of input.";
    return reader.take_lines();
}
} // namespace

TEST(BackgroundLineReader, ReadsLinesInOrderWithoutTheirNewlines)
{
    const std::vector<std::string> lines = lines_read_from("first\nsecond\nthird\n");

    const std::vector<std::string> expected = { "first", "second", "third" };
    EXPECT_EQ(lines, expected) << "Expected every line, in order, without newlines.";
}

TEST(BackgroundLineReader, ReadsALastLineWithoutANewline)
{
    const std::vector<std::string> lines = lines_read_from("first\nlast");

    const std::vector<std::string> expected = { "first", "last" };
    EXPECT_EQ(lines, expected) << "Expected the unterminated last line.";
}

TEST(BackgroundLineReader, AddsNoLineAfterAFinalNewline)
{
    const std::vector<std::string> lines = lines_read_from("first\nlast\n");

    const std::vector<std::string> expected = { "first", "last" };
    EXPECT_EQ(lines, expected) << "Expected no empty line after the final newline.";
}

TEST(BackgroundLineReader, LeavesACarriageReturnForTheDispatcherToRemove)
{
    const std::vector<std::string> lines = lines_read_from("first\r\n");

    const std::vector<std::string> expected = { "first\r" };
    EXPECT_EQ(lines, expected) << "Expected the CR to stay on the line.";
}

TEST(BackgroundLineReader, ReadsAnEmptyLineAsAnEmptyString)
{
    const std::vector<std::string> lines = lines_read_from("\nnext\n");

    const std::vector<std::string> expected = { "", "next" };
    EXPECT_EQ(lines, expected) << "Expected the empty line to be delivered.";
}

TEST(BackgroundLineReader, DeliversALongLineWhole)
{
    const std::string long_line(LONG_LINE_LENGTH, 'a');
    const std::vector<std::string> lines = lines_read_from(long_line + "\nnext\n");

    const std::vector<std::string> expected = { long_line, "next" };
    EXPECT_EQ(lines, expected) << "Expected the long line whole, then the next line.";
}

TEST(BackgroundLineReader, ReportsInputClosedAtTheEndOfInput)
{
    std::istringstream input("only\n");
    harness::BackgroundLineReader reader(input);

    EXPECT_TRUE(wait_until_input_closed(reader)) << "Expected input closed after its end.";
}

TEST(BackgroundLineReader, ReportsInputClosedAtTheEndOfEmptyInput)
{
    std::istringstream input("");
    harness::BackgroundLineReader reader(input);

    ASSERT_TRUE(wait_until_input_closed(reader)) << "Expected input closed on empty input.";
    EXPECT_TRUE(reader.take_lines().empty()) << "Expected no line from empty input.";
}

TEST(BackgroundLineReader, TakingLinesEmptiesTheQueue)
{
    std::istringstream input("only\n");
    harness::BackgroundLineReader reader(input);
    ASSERT_TRUE(wait_until_input_closed(reader)) << "Expected the reader to reach end of input.";

    EXPECT_EQ(reader.take_lines().size(), 1u) << "Expected the line on the first take.";
    EXPECT_TRUE(reader.take_lines().empty()) << "Expected nothing on the second take.";
}
