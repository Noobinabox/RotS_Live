/* harness_controlled_schedule.cpp */

#include "harness_controlled_schedule.h"

#include "harness_request.h"
#include "utils.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace harness {

//============================================================================
HarnessControlledSchedule::HarnessControlledSchedule(std::unique_ptr<HarnessControlChannel> channel)
    : control_channel(std::move(channel))
{
    if (control_channel == nullptr) {
        const std::string message = std::string("SYSERR: ") + __func__
            + ": no control channel; no game hours will be granted.";
        log(message.c_str());
    }
}

//============================================================================
GameTickSchedule::WorldPass HarnessControlledSchedule::next_pass()
{
    if (control_channel != nullptr) {
        if (!ready_sent) {
            const std::string ready = "ready schedule=" + std::string(AffectHoursSchedule::NAME)
                + " hours=" + std::to_string(hours_schedule.hours_completed());
            control_channel->write_reply(ready);
            ready_sent = true;
        }

        acknowledge_finished_requests();
        const std::vector<std::string> lines = control_channel->take_lines();
        for (const std::string& line : lines) {
            apply_request(line);
        }
        report_closed_input();
    }

    const WorldPass world_pass = hours_schedule.next_pass();
    passes_returned++;
    return world_pass;
}

//============================================================================
void HarnessControlledSchedule::acknowledge_finished_requests()
{
    while (!pending_acknowledgements.empty()
        && pending_acknowledgements.front().last_pass <= passes_returned) {
        const PendingAcknowledgement finished = pending_acknowledgements.front();
        pending_acknowledgements.pop_front();
        const std::string done = "done " + std::string(ADVANCE_HOURS_REQUEST) + " "
            + std::to_string(finished.hours)
            + " hours=" + std::to_string(hours_schedule.hours_completed());
        control_channel->write_reply(done);
    }
}

//============================================================================
void HarnessControlledSchedule::apply_request(std::string_view line)
{
    int hours = 0;
    std::string problem;
    if (!parse_harness_request(line, hours, problem)) {
        control_channel->write_reply("error " + problem);
        return;
    }

    const std::optional<std::string> refusal = hours_schedule.grant_hours(hours);
    if (refusal.has_value()) {
        const std::string error = "error " + std::string(ADVANCE_HOURS_REQUEST) + " "
            + std::to_string(hours) + ": " + *refusal;
        control_channel->write_reply(error);
        return;
    }

    const int pending_passes = hours_schedule.pending_passes();
    PendingAcknowledgement acknowledgement;
    acknowledgement.last_pass = passes_returned + static_cast<std::uint64_t>(pending_passes);
    acknowledgement.hours = hours;
    pending_acknowledgements.push_back(acknowledgement);
}

//============================================================================
void HarnessControlledSchedule::report_closed_input()
{
    if (closed_input_reported || !control_channel->input_closed()) {
        return;
    }

    closed_input_reported = true;
    log("Harness: the control channel's input has closed; no more time requests will arrive.");
}

} // namespace harness
