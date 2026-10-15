/* standard_stream_channel.cpp */

#include "standard_stream_channel.h"

#include "request_dispatch.h"
#include "utils.h"

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <limits>

namespace harness {

namespace {
// One character past the longest request, plus getline's terminating null, so an overlong line
// shows up as a full buffer instead of being read whole.
constexpr std::size_t LINE_BUFFER_SIZE = MAX_REQUEST_LINE_LENGTH + 2;
static_assert(LINE_BUFFER_SIZE
        <= static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()),
    "The line buffer's size fits a streamsize.");
} // namespace

//============================================================================
StandardStreamChannel::StandardStreamChannel(std::istream& input, std::ostream& output)
    : received_input(std::make_shared<ReceivedInput>())
    , reply_output(output)
    , reader(&StandardStreamChannel::read_lines, std::ref(input), received_input)
{
}

//============================================================================
StandardStreamChannel::~StandardStreamChannel()
{
    if (!reader.joinable()) {
        return;
    }

    if (input_closed()) {
        reader.join();
        return;
    }

    // A thread blocked reading a stream cannot be interrupted portably; it finishes when input
    // ends, holding its own share of the received input.
    reader.detach();
}

//============================================================================
void StandardStreamChannel::read_lines(std::istream& input, std::shared_ptr<ReceivedInput> received)
{
    std::array<char, LINE_BUFFER_SIZE> buffer {};
    while (true) {
        input.getline(buffer.data(), static_cast<std::streamsize>(LINE_BUFFER_SIZE));
        if (input.bad()) {
            break;
        }

        // gcount() is never negative and at most LINE_BUFFER_SIZE - 1 here.
        std::size_t line_length = static_cast<std::size_t>(input.gcount());
        const bool reached_end = input.eof();
        if (reached_end && line_length == 0) {
            break;
        }

        if (!reached_end) {
            if (input.fail()) {
                // The buffer filled before a newline: keep what was read, discard the rest.
                input.clear();
                input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            } else {
                // gcount() counted the newline getline removed.
                line_length -= 1;
            }
        }

        {
            std::lock_guard<std::mutex> lock(received->guard);
            received->lines.emplace_back(buffer.data(), line_length);
        }

        if (reached_end) {
            break;
        }
    }

    std::lock_guard<std::mutex> lock(received->guard);
    received->closed = true;
}

//============================================================================
std::vector<std::string> StandardStreamChannel::take_lines()
{
    std::lock_guard<std::mutex> lock(received_input->guard);
    std::vector<std::string> taken;
    taken.reserve(received_input->lines.size());
    std::move(
        received_input->lines.begin(), received_input->lines.end(), std::back_inserter(taken));
    received_input->lines.clear();
    return taken;
}

//============================================================================
void StandardStreamChannel::write_reply(std::string_view text)
{
    if (output_failed) {
        return;
    }

    reply_output << REPLY_PREFIX << text << '\n' << std::flush;
    if (!reply_output) {
        output_failed = true;
        const std::string message = std::string("SYSERR: ") + __func__
            + ": a harness reply could not be written; no further replies will be sent.";
        log(message.c_str());
    }
}

//============================================================================
bool StandardStreamChannel::input_closed() const
{
    std::lock_guard<std::mutex> lock(received_input->guard);
    return received_input->closed;
}

} // namespace harness
