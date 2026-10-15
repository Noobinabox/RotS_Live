/* advance_hours_request.cpp */

#include "advance_hours_request.h"

#include "utils.h"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace harness {

namespace {
// Whether character is a decimal digit; std::isdigit takes an int and depends on the locale.
bool is_decimal_digit(char character)
{
    return character >= '0' && character <= '9';
}

// Reads hours_text as an hour count from 1 to AdvanceHoursRequest::MAX_REQUESTED_HOURS, or
// returns an empty value when it is not one.
std::optional<int> read_hour_count(std::string_view hours_text)
{
    // from_chars would accept a minus sign, and a leading zero is refused as padding, so the
    // count must start with a non-zero digit.
    if (hours_text.empty() || !is_decimal_digit(hours_text.front()) || hours_text.front() == '0') {
        return std::nullopt;
    }

    int hours = 0;
    const char* const hours_end = hours_text.data() + hours_text.size();
    const std::from_chars_result parsed = std::from_chars(hours_text.data(), hours_end, hours);
    if (parsed.ec != std::errc() || parsed.ptr != hours_end
        || hours > AdvanceHoursRequest::MAX_REQUESTED_HOURS) {
        return std::nullopt;
    }
    return hours;
}
} // namespace

//============================================================================
AdvanceHoursRequest::AdvanceHoursRequest(AffectHoursSchedule& schedule)
    : hours_schedule(schedule)
{
}

//============================================================================
std::string_view AdvanceHoursRequest::name() const
{
    return NAME;
}

//============================================================================
RequestOutcome AdvanceHoursRequest::handle(std::string_view line, std::string_view arguments)
{
    const std::optional<int> requested_hours = read_hour_count(arguments);
    if (!requested_hours.has_value()) {
        std::string refusal = "error " + std::string(line)
            + ": hours must be a whole number from 1 to " + std::to_string(MAX_REQUESTED_HOURS);
        return ImmediateReply { std::move(refusal) };
    }

    const int hours = *requested_hours;
    const std::optional<std::string> schedule_refusal = hours_schedule.grant_hours(hours);
    if (schedule_refusal.has_value()) {
        std::string refusal = "error " + std::string(NAME) + " " + std::to_string(hours) + ": "
            + *schedule_refusal;
        return ImmediateReply { std::move(refusal) };
    }

    // The reply waits for the whole backlog, so a grant queued behind another is acknowledged
    // only once both have run.
    const int backlog_passes = hours_schedule.pending_passes();
    DeferredReply acknowledgement;
    if (backlog_passes >= 1) {
        acknowledgement.pass_count = static_cast<std::uint64_t>(backlog_passes);
    } else {
        const std::string message = std::string("SYSERR: ") + __func__
            + ": a grant left no passes pending; acknowledging it after one pass.";
        log(message.c_str());
    }

    // Borrow the schedule rather than this request, so the body depends only on the schedule the
    // reply reports.
    AffectHoursSchedule& schedule = hours_schedule;
    acknowledgement.build_body = [&schedule, hours]() -> std::string {
        return "done " + std::string(NAME) + " " + std::to_string(hours)
            + " hours=" + std::to_string(schedule.hours_completed());
    };
    return acknowledgement;
}

} // namespace harness
