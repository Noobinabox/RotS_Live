/* harness_request.cpp */

#include "harness_request.h"

#include <charconv>
#include <string>
#include <system_error>

namespace harness {

namespace {
// Sets out_error to line, a colon and problem.
void describe_refusal(std::string_view line, std::string_view problem, std::string& out_error)
{
    out_error.assign(line);
    out_error.append(": ");
    out_error.append(problem);
}

// Whether character is a decimal digit; std::isdigit takes an int and depends on the locale.
bool is_decimal_digit(char character)
{
    return character >= '0' && character <= '9';
}
} // namespace

//============================================================================
bool parse_harness_request(std::string_view line, int& out_hours, std::string& out_error)
{
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }

    if (line.size() > MAX_REQUEST_LINE_LENGTH) {
        out_error
            = "request longer than " + std::to_string(MAX_REQUEST_LINE_LENGTH) + " characters";
        return false;
    }

    if (line.empty()) {
        out_error = "empty request";
        return false;
    }

    const std::size_t space_position = line.find(' ');
    const std::string_view request_name = line.substr(0, space_position);
    if (request_name != ADVANCE_HOURS_REQUEST) {
        describe_refusal(line, "unknown request", out_error);
        return false;
    }

    const std::string bad_hours_problem
        = "hours must be a whole number from 1 to " + std::to_string(MAX_REQUESTED_HOURS);
    if (space_position == std::string_view::npos) {
        describe_refusal(line, bad_hours_problem, out_error);
        return false;
    }

    // from_chars would accept a minus sign, and a leading zero is refused as padding, so the
    // count must start with a non-zero digit.
    const std::string_view hours_text = line.substr(space_position + 1);
    if (hours_text.empty() || !is_decimal_digit(hours_text.front()) || hours_text.front() == '0') {
        describe_refusal(line, bad_hours_problem, out_error);
        return false;
    }

    int hours = 0;
    const char* const hours_end = hours_text.data() + hours_text.size();
    const std::from_chars_result parsed = std::from_chars(hours_text.data(), hours_end, hours);
    if (parsed.ec != std::errc() || parsed.ptr != hours_end || hours > MAX_REQUESTED_HOURS) {
        describe_refusal(line, bad_hours_problem, out_error);
        return false;
    }

    out_hours = hours;
    return true;
}

} // namespace harness
