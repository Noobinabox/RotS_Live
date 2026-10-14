/* harness_request.h */

#ifndef HARNESS_REQUEST_H
#define HARNESS_REQUEST_H

#include <cstddef>
#include <string>
#include <string_view>

namespace harness {

// The request that asks for game hours to run, followed by a space and the hour count.
inline constexpr std::string_view ADVANCE_HOURS_REQUEST = "advance-hours";
// The most game hours one request may ask for.
inline constexpr int MAX_REQUESTED_HOURS = 1000;
// The longest request line accepted, in characters, not counting its line end.
inline constexpr std::size_t MAX_REQUEST_LINE_LENGTH = 64;

// Reads one request line: ADVANCE_HOURS_REQUEST, one space, and an hour count from 1 to
// MAX_REQUESTED_HOURS in decimal digits, with no sign or leading zero. One trailing carriage
// return is ignored. Returns true and sets out_hours for such a line. Otherwise leaves
// out_hours unchanged, sets out_error to "<line>: <problem>", or to "empty request" or
// "request longer than ..." without quoting the line, and returns false.
bool parse_harness_request(std::string_view line, int& out_hours, std::string& out_error);

} // namespace harness

#endif /* HARNESS_REQUEST_H */
