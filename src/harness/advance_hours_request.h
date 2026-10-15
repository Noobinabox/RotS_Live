/* advance_hours_request.h */

#ifndef ADVANCE_HOURS_REQUEST_H
#define ADVANCE_HOURS_REQUEST_H

#include "affect_hours_schedule.h"
#include "harness_request.h"
#include "request_outcome.h"

#include <string_view>

namespace harness {

// The request that grants game hours on an AffectHoursSchedule: its name, one space, and an hour
// count.
class AdvanceHoursRequest : public HarnessRequest {
public:
    // The request's name, the first word of its line.
    static constexpr std::string_view NAME = "advance-hours";
    // The most game hours one request may ask for.
    static constexpr int MAX_REQUESTED_HOURS = 1000;

    // Grants hours on schedule, which must outlive the request and every reply it defers.
    explicit AdvanceHoursRequest(AffectHoursSchedule& schedule);

    // Returns NAME.
    [[nodiscard]] std::string_view name() const override;

    // Grants the hours arguments asks for: an hour count from 1 to MAX_REQUESTED_HOURS in decimal
    // digits, with no sign, padding or leading zero.
    // - A granted request is answered "done advance-hours N hours=H" once every pass the
    //   schedule has pending after the grant, this grant's included, has been returned. H is the
    //   schedule's hours_completed() when the reply is sent.
    // - A malformed count is answered now with "error <line>: hours must be a whole number from 1
    //   to <MAX_REQUESTED_HOURS>", leaving the schedule unchanged.
    // - Hours the schedule refuses are answered now with "error advance-hours N: <its reason>".
    [[nodiscard]] RequestOutcome handle(std::string_view line, std::string_view arguments) override;

private:
    // Where granted hours go; borrowed.
    AffectHoursSchedule& hours_schedule;
};

} // namespace harness

#endif /* ADVANCE_HOURS_REQUEST_H */
