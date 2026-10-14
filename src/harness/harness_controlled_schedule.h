/* harness_controlled_schedule.h */

#ifndef HARNESS_CONTROLLED_SCHEDULE_H
#define HARNESS_CONTROLLED_SCHEDULE_H

#include "affect_hours_schedule.h"
#include "game_tick_schedule.h"
#include "harness_control_channel.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string_view>

namespace harness {

// Runs an AffectHoursSchedule on the requests that arrive over a control channel, and
// acknowledges each request once the passes it granted have run.
class HarnessControlledSchedule : public GameTickSchedule {
public:
    // Takes ownership of channel. A null channel is logged once, and nothing is then granted.
    explicit HarnessControlledSchedule(std::unique_ptr<HarnessControlChannel> channel);

    // Answers the channel, then returns the AffectHoursSchedule's pass.
    // - The first call replies "ready schedule=<AffectHoursSchedule::NAME> hours=0".
    // - Each call replies "done advance-hours N hours=H" for every request whose last pass the
    //   previous call returned, H counting all hours run so far.
    // - Each line received is granted from the pass this call returns, or answered
    //   "error <problem>".
    // - Logs once when the channel's input closes; hours already granted still run.
    [[nodiscard]] WorldPass next_pass() override;

private:
    // A granted request waiting for its acknowledgement.
    struct PendingAcknowledgement {
        // The count of passes returned at which the request's last pass has been returned.
        std::uint64_t last_pass = 0;
        // The hours the request asked for.
        int hours = 0;
    };

    // Replies "done" for every pending request whose last pass has been returned.
    void acknowledge_finished_requests();
    // Grants or refuses one received line.
    void apply_request(std::string_view line);
    // Logs, once, that the channel's input has closed.
    void report_closed_input();

    // Where requests come from and replies go; owned, and null only when constructed so.
    std::unique_ptr<HarnessControlChannel> control_channel;
    // Decides each pass from the hours granted.
    AffectHoursSchedule hours_schedule;
    // Granted requests not yet acknowledged, oldest first.
    std::deque<PendingAcknowledgement> pending_acknowledgements;
    // Passes returned so far.
    std::uint64_t passes_returned = 0;
    // True once "ready" has been sent.
    bool ready_sent = false;
    // True once the closed input has been logged.
    bool closed_input_reported = false;
};

} // namespace harness

#endif /* HARNESS_CONTROLLED_SCHEDULE_H */
