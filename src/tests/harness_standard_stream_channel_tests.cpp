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
// A line far longer than any request, to show the channel does not cut lines.
constexpr std::size_t LONG_LINE_LENGTH = 200;
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

TEST(StandardStreamChannel, ReceivesALongLineWhole)
{
    const std::string long_line(LONG_LINE_LENGTH, 'a');
    const std::vector<std::string> lines = lines_received_from(long_line + "\nnext\n");

    const std::vector<std::string> expected = { long_line, "next" };
    EXPECT_EQ(lines, expected) << "Expected the long line whole, then the next line, in order.";
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
