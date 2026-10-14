/* standard_stream_channel.h */

#ifndef STANDARD_STREAM_CHANNEL_H
#define STANDARD_STREAM_CHANNEL_H

#include "harness_control_channel.h"

#include <deque>
#include <istream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace harness {

// The text that starts every reply line, so the harness can tell replies from other output.
inline constexpr std::string_view REPLY_PREFIX = "harness: ";

// The control channel over a pair of streams, normally the process's standard input and
// output. A thread of its own reads input; every member function runs on the caller's thread.
class StandardStreamChannel : public HarnessControlChannel {
public:
    // Starts reading input on the channel's thread. output must outlive the channel. input
    // must stay valid until it ends, even after the channel is destroyed.
    StandardStreamChannel(std::istream& input, std::ostream& output);

    // Joins the reading thread if input has already ended; otherwise, as at shutdown, leaves it
    // to end with the process.
    ~StandardStreamChannel() override;

    // A copy would share the reading thread.
    StandardStreamChannel(const StandardStreamChannel&) = delete;
    StandardStreamChannel& operator=(const StandardStreamChannel&) = delete;

    // See HarnessControlChannel. A line longer than MAX_REQUEST_LINE_LENGTH + 1 characters
    // arrives cut to that length, and the rest of it is discarded.
    [[nodiscard]] std::vector<std::string> take_lines() override;

    // Writes REPLY_PREFIX, text and a newline, then flushes. Once output has failed, logs that
    // once and writes nothing more.
    void write_reply(std::string_view text) override;

    // See HarnessControlChannel.
    [[nodiscard]] bool input_closed() const override;

private:
    // What the reading thread shares with the channel. Shared ownership lets a reading thread
    // that outlives the channel keep using it.
    struct ReceivedInput {
        // Guards lines and closed.
        std::mutex guard;
        // Lines received and not yet taken, oldest first.
        std::deque<std::string> lines;
        // True once input has ended or failed.
        bool closed = false;
    };

    // Reads lines from input into received until input ends or fails, then marks it closed.
    static void read_lines(std::istream& input, std::shared_ptr<ReceivedInput> received);

    // The lines and end-of-input state the reading thread fills.
    std::shared_ptr<ReceivedInput> received_input;
    // Where replies go; borrowed.
    std::ostream& reply_output;
    // True once a reply could not be written; later replies are dropped.
    bool output_failed = false;
    // Runs read_lines; declared last so it starts after the members it uses.
    std::thread reader;
};

} // namespace harness

#endif /* STANDARD_STREAM_CHANNEL_H */
