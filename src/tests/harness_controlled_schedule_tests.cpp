#include "affect_hours_schedule.h"
#include "harness_control_channel.h"
#include "harness_controlled_schedule.h"
#include "harness_request.h"

#include "../structs.h"

#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
// The words the schedule logs when the channel's input closes.
constexpr std::string_view CLOSED_INPUT_PHRASE = "no more time requests";
// The words the schedule logs when it has no channel.
constexpr std::string_view NO_CHANNEL_PHRASE = "no control channel";

// What a FakeControlChannel shares with its test.
struct FakeChannelState {
    // Lines the next take_lines() returns.
    std::vector<std::string> lines_to_deliver;
    // Every reply written, in order.
    std::vector<std::string> replies;
    // What input_closed() reports.
    bool closed = false;
};

// A control channel whose lines, replies and closed state a test controls through shared state.
class FakeControlChannel : public harness::HarnessControlChannel {
public:
    explicit FakeControlChannel(std::shared_ptr<FakeChannelState> state)
        : shared_state(std::move(state))
    {
    }

    [[nodiscard]] std::vector<std::string> take_lines() override
    {
        std::vector<std::string> taken;
        taken.swap(shared_state->lines_to_deliver);
        return taken;
    }

    void write_reply(std::string_view text) override
    {
        shared_state->replies.emplace_back(text);
    }

    [[nodiscard]] bool input_closed() const override
    {
        return shared_state->closed;
    }

private:
    // The state the test reads and changes.
    std::shared_ptr<FakeChannelState> shared_state;
};

// A schedule wired to a fake channel, and the state that drives it.
struct ScheduleUnderTest {
    // What the test feeds and inspects.
    std::shared_ptr<FakeChannelState> channel_state = std::make_shared<FakeChannelState>();
    // The schedule, owning its fake channel.
    harness::HarnessControlledSchedule schedule {
        std::make_unique<FakeControlChannel>(channel_state)
    };
};

// The ready reply the schedule sends on its first pass.
std::string ready_reply()
{
    return "ready schedule=" + std::string(harness::AffectHoursSchedule::NAME) + " hours=0";
}

// The request line asking for hours game hours.
std::string advance_request(int hours)
{
    return std::string(harness::ADVANCE_HOURS_REQUEST) + " " + std::to_string(hours);
}

// The acknowledgement for a request of hours, with total hours run so far.
std::string done_reply(int hours, int total_hours)
{
    return "done " + advance_request(hours) + " hours=" + std::to_string(total_hours);
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
} // namespace

TEST(HarnessControlledSchedule, SendsReadyOnceOnTheFirstPass)
{
    ScheduleUnderTest under_test;

    for (int pass_number = 1; pass_number <= 3; ++pass_number) {
        static_cast<void>(under_test.schedule.next_pass());
    }

    const std::vector<std::string> expected = { ready_reply() };
    EXPECT_EQ(under_test.channel_state->replies, expected) << "Expected exactly one ready reply.";
}

TEST(HarnessControlledSchedule, RunsNoWorldUpdateUntilARequestArrives)
{
    ScheduleUnderTest under_test;

    for (int pass_number = 1; pass_number <= FAST_UPDATE_RATE; ++pass_number) {
        const GameTickSchedule::WorldPass world_pass = under_test.schedule.next_pass();
        EXPECT_FALSE(world_pass.run_fast_update) << "Expected an idle pass " << pass_number;
    }
}

TEST(HarnessControlledSchedule, AppliesARequestFromThePassOnWhichItArrives)
{
    ScheduleUnderTest under_test;
    for (int pass_number = 1; pass_number <= 4; ++pass_number) {
        static_cast<void>(under_test.schedule.next_pass());
    }

    under_test.channel_state->lines_to_deliver.push_back(advance_request(1));
    const GameTickSchedule::WorldPass world_pass = under_test.schedule.next_pass();

    EXPECT_TRUE(world_pass.run_fast_update) << "Expected the request's first pass at once.";
}

TEST(HarnessControlledSchedule, AppliesARequestReceivedBeforeTheFirstPassAfterSendingReady)
{
    ScheduleUnderTest under_test;
    under_test.channel_state->lines_to_deliver.push_back(advance_request(1));

    const GameTickSchedule::WorldPass world_pass = under_test.schedule.next_pass();

    EXPECT_TRUE(world_pass.run_fast_update) << "Expected the early request on the first pass.";
    ASSERT_FALSE(under_test.channel_state->replies.empty()) << "Expected a reply.";
    EXPECT_EQ(under_test.channel_state->replies.front(), ready_reply())
        << "Expected ready before anything else.";
}

TEST(HarnessControlledSchedule, AcknowledgesARequestOnThePassAfterItsLastPassRuns)
{
    ScheduleUnderTest under_test;
    under_test.channel_state->lines_to_deliver.push_back(advance_request(1));

    for (int pass_number = 1; pass_number <= FAST_UPDATE_RATE; ++pass_number) {
        static_cast<void>(under_test.schedule.next_pass());
    }
    EXPECT_EQ(under_test.channel_state->replies.size(), 1u)
        << "Expected no acknowledgement while the hour's last pass is still to run.";

    const GameTickSchedule::WorldPass after_hour = under_test.schedule.next_pass();

    EXPECT_FALSE(after_hour.run_fast_update) << "Expected the pass after the hour to be idle.";
    ASSERT_EQ(under_test.channel_state->replies.size(), 2u) << "Expected the acknowledgement.";
    EXPECT_EQ(under_test.channel_state->replies.back(), done_reply(1, 1))
        << "Unexpected acknowledgement text.";
}

TEST(HarnessControlledSchedule, AcknowledgesQueuedRequestsInOrderOnceEachHasRun)
{
    ScheduleUnderTest under_test;
    under_test.channel_state->lines_to_deliver = { advance_request(1), advance_request(2) };
    std::vector<int> passes_at_reply;
    std::size_t replies_seen = 1;

    for (int pass_number = 1; pass_number <= 3 * FAST_UPDATE_RATE + 1; ++pass_number) {
        static_cast<void>(under_test.schedule.next_pass());
        if (under_test.channel_state->replies.size() > replies_seen) {
            passes_at_reply.push_back(pass_number);
            replies_seen = under_test.channel_state->replies.size();
        }
    }

    const std::vector<std::string> expected
        = { ready_reply(), done_reply(1, 1), done_reply(2, 3) };
    EXPECT_EQ(under_test.channel_state->replies, expected)
        << "Expected both acknowledgements, in order.";
    const std::vector<int> expected_passes = { FAST_UPDATE_RATE + 1, 3 * FAST_UPDATE_RATE + 1 };
    EXPECT_EQ(passes_at_reply, expected_passes)
        << "Expected each acknowledgement on the pass after its request's last pass.";
}

TEST(HarnessControlledSchedule, RepliesWithTheParsersErrorAndKeepsRunning)
{
    ScheduleUnderTest under_test;
    const std::string bad_line = std::string(harness::ADVANCE_HOURS_REQUEST) + " x";
    under_test.channel_state->lines_to_deliver.push_back(bad_line);

    const GameTickSchedule::WorldPass world_pass = under_test.schedule.next_pass();

    int unused_hours = 0;
    std::string parser_error;
    ASSERT_FALSE(harness::parse_harness_request(bad_line, unused_hours, parser_error))
        << "Expected the parser to refuse: " << bad_line;
    ASSERT_EQ(under_test.channel_state->replies.size(), 2u) << "Expected ready and an error.";
    EXPECT_EQ(under_test.channel_state->replies.back(), "error " + parser_error)
        << "Expected the parser's refusal as the reply.";
    EXPECT_FALSE(world_pass.run_fast_update) << "Expected a refused request to grant nothing.";

    under_test.channel_state->lines_to_deliver.push_back(advance_request(1));
    const GameTickSchedule::WorldPass next_pass = under_test.schedule.next_pass();
    EXPECT_TRUE(next_pass.run_fast_update) << "Expected a later valid request to run.";
}

TEST(HarnessControlledSchedule, RepliesWithAnErrorWhenTheScheduleRefusesTheHours)
{
    ScheduleUnderTest under_test;
    const int maximum = harness::AffectHoursSchedule::MAX_PENDING_HOURS;
    under_test.channel_state->lines_to_deliver = { advance_request(maximum), advance_request(1) };

    static_cast<void>(under_test.schedule.next_pass());

    harness::AffectHoursSchedule reference;
    static_cast<void>(reference.grant_hours(maximum));
    const std::optional<std::string> refusal = reference.grant_hours(1);
    ASSERT_TRUE(refusal.has_value()) << "Expected the reference schedule to refuse.";
    ASSERT_EQ(under_test.channel_state->replies.size(), 2u) << "Expected ready and an error.";
    EXPECT_EQ(under_test.channel_state->replies.back(),
        "error " + advance_request(1) + ": " + *refusal)
        << "Expected the schedule's refusal as the reply.";
}

TEST(HarnessControlledSchedule, FinishesAndAcknowledgesAGrantAfterTheInputCloses)
{
    ScheduleUnderTest under_test;
    under_test.channel_state->lines_to_deliver.push_back(advance_request(1));
    static_cast<void>(under_test.schedule.next_pass());
    under_test.channel_state->closed = true;

    testing::internal::CaptureStderr();
    for (int pass_number = 2; pass_number <= FAST_UPDATE_RATE + 1; ++pass_number) {
        static_cast<void>(under_test.schedule.next_pass());
    }
    static_cast<void>(testing::internal::GetCapturedStderr());

    EXPECT_EQ(under_test.channel_state->replies.back(), done_reply(1, 1))
        << "Expected the grant to finish and be acknowledged after input closed.";
}

TEST(HarnessControlledSchedule, LogsOnceWhenTheChannelInputCloses)
{
    ScheduleUnderTest under_test;
    under_test.channel_state->closed = true;

    testing::internal::CaptureStderr();
    for (int pass_number = 1; pass_number <= 3; ++pass_number) {
        static_cast<void>(under_test.schedule.next_pass());
    }
    const std::string logged = testing::internal::GetCapturedStderr();

    EXPECT_EQ(count_occurrences(logged, CLOSED_INPUT_PHRASE), 1)
        << "Expected the closed input logged once; log was: " << logged;
}

TEST(HarnessControlledSchedule, WithANullChannelLogsAndRunsNoWorldUpdate)
{
    testing::internal::CaptureStderr();
    harness::HarnessControlledSchedule schedule(nullptr);
    const GameTickSchedule::WorldPass world_pass = schedule.next_pass();
    const std::string logged = testing::internal::GetCapturedStderr();

    EXPECT_FALSE(world_pass.run_fast_update) << "Expected no world update without a channel.";
    EXPECT_EQ(count_occurrences(logged, NO_CHANNEL_PHRASE), 1)
        << "Expected the missing channel logged once; log was: " << logged;
}
