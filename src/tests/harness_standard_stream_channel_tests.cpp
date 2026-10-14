#include "harness_request.h"
#include "standard_stream_channel.h"

#include <chrono>
#include <gtest/gtest.h>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
// How long a test waits for the reader thread to reach the end of a string stream.
constexpr std::chrono::seconds READER_WAIT_LIMIT(5);
// The words write_reply logs once output has failed.
constexpr std::string_view OUTPUT_FAILURE_PHRASE = "could not be written";

// Waits until channel reports its input closed; returns false after READER_WAIT_LIMIT.
bool wait_until_input_closed(const harness::StandardStreamChannel& channel)
{
    const std::chrono::steady_clock::time_point deadline
        = std::chrono::steady_clock::now() + READER_WAIT_LIMIT;
    while (!channel.input_closed()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

// Reads all of input through a channel and returns the lines it received.
std::vector<std::string> lines_received_from(const std::string& input_text)
{
    std::istringstream input(input_text);
    std::ostringstream output;
    harness::StandardStreamChannel channel(input, output);
    EXPECT_TRUE(wait_until_input_closed(channel)) << "Expected the reader to reach end of input.";
    return channel.take_lines();
}

// Returns how many times phrase occurs in text.
int count_occurrences(std::string_view text, std::string_view phrase)
{
    int occurrences = 0;
    std::size_t position = text.find(phrase);
    while (position != std::string_view::npos) {
        occurrences++;
        position = text.find(phrase, position + phrase.size());
    }
    return occurrences;
}

// A string buffer that counts how often it is flushed.
class CountingBuffer : public std::stringbuf {
public:
    // How many times the stream has been flushed.
    int flushes = 0;

protected:
    int sync() override
    {
        flushes++;
        return std::stringbuf::sync();
    }
};
} // namespace

TEST(StandardStreamChannel, ReceivesLinesInOrderWithoutTheirNewlines)
{
    const std::vector<std::string> lines = lines_received_from("first\nsecond\n");

    const std::vector<std::string> expected = { "first", "second" };
    EXPECT_EQ(lines, expected) << "Expected both lines, in order, without newlines.";
}

TEST(StandardStreamChannel, LeavesACarriageReturnForTheParserToRemove)
{
    const std::vector<std::string> lines = lines_received_from("first\r\n");

    const std::vector<std::string> expected = { "first\r" };
    EXPECT_EQ(lines, expected) << "Expected the CR to stay on the line.";
}

TEST(StandardStreamChannel, ReceivesALastLineWithoutANewline)
{
    const std::vector<std::string> lines = lines_received_from("first\nlast");

    const std::vector<std::string> expected = { "first", "last" };
    EXPECT_EQ(lines, expected) << "Expected the unterminated last line.";
}

TEST(StandardStreamChannel, ReceivesAnEmptyLineAsAnEmptyString)
{
    const std::vector<std::string> lines = lines_received_from("\nnext\n");

    const std::vector<std::string> expected = { "", "next" };
    EXPECT_EQ(lines, expected) << "Expected the empty line to be delivered.";
}

TEST(StandardStreamChannel, CutsAnOverlongLineOneCharacterPastTheLimitAndKeepsTheNextLine)
{
    const std::string overlong(harness::MAX_REQUEST_LINE_LENGTH + 10, 'a');
    const std::vector<std::string> lines = lines_received_from(overlong + "\nnext\n");

    ASSERT_EQ(lines.size(), 2u) << "Expected the cut line and the next line.";
    EXPECT_EQ(lines[0], std::string(harness::MAX_REQUEST_LINE_LENGTH + 1, 'a'))
        << "Expected the overlong line cut one character past the limit.";
    EXPECT_EQ(lines[1], "next") << "Expected the following line intact.";
}

TEST(StandardStreamChannel, ReceivesALineOneCharacterPastTheLimitWhole)
{
    const std::string just_over(harness::MAX_REQUEST_LINE_LENGTH + 1, 'b');
    const std::vector<std::string> lines = lines_received_from(just_over + "\nnext\n");

    const std::vector<std::string> expected = { just_over, "next" };
    EXPECT_EQ(lines, expected) << "Expected a line that fits the buffer to arrive whole.";
}

TEST(StandardStreamChannel, ReportsInputClosedAtTheEndOfEmptyInput)
{
    const std::vector<std::string> lines = lines_received_from("");

    EXPECT_TRUE(lines.empty()) << "Expected no line from empty input.";
}

TEST(StandardStreamChannel, GivesEachLineOutOnlyOnce)
{
    std::istringstream input("only\n");
    std::ostringstream output;
    harness::StandardStreamChannel channel(input, output);
    ASSERT_TRUE(wait_until_input_closed(channel)) << "Expected the reader to reach end of input.";

    EXPECT_EQ(channel.take_lines().size(), 1u) << "Expected the line on the first take.";
    EXPECT_TRUE(channel.take_lines().empty()) << "Expected nothing on the second take.";
}

TEST(StandardStreamChannel, WritesEachReplyWithThePrefixAndANewlineAndFlushes)
{
    std::istringstream input("");
    CountingBuffer counting_buffer;
    std::ostream output(&counting_buffer);
    harness::StandardStreamChannel channel(input, output);
    ASSERT_TRUE(wait_until_input_closed(channel)) << "Expected the reader to reach end of input.";

    channel.write_reply("ready");
    channel.write_reply("done");

    EXPECT_EQ(counting_buffer.str(),
        std::string(harness::REPLY_PREFIX) + "ready\n" + std::string(harness::REPLY_PREFIX)
            + "done\n")
        << "Expected each reply on its own prefixed line.";
    EXPECT_GE(counting_buffer.flushes, 2) << "Expected a flush after each reply.";
}

TEST(StandardStreamChannel, StopsWritingAndLogsOnceWhenOutputFails)
{
    std::istringstream input("");
    std::ostringstream output;
    output.setstate(std::ios::badbit);
    harness::StandardStreamChannel channel(input, output);
    ASSERT_TRUE(wait_until_input_closed(channel)) << "Expected the reader to reach end of input.";

    testing::internal::CaptureStderr();
    channel.write_reply("first");
    channel.write_reply("second");
    const std::string logged = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(output.str().empty()) << "Expected nothing written to a failed stream.";
    EXPECT_EQ(count_occurrences(logged, OUTPUT_FAILURE_PHRASE), 1)
        << "Expected the failure logged once; log was: " << logged;
}
